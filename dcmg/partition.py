from __future__ import annotations

from dataclasses import dataclass
import numpy as np
import scipy.sparse as sp

from .elements import BuckConverter, Line


def bus_graph(sys, conv_weight=4, line_weight=1):
    net = sys.network
    nb = sys.n_nodes
    w = np.ones(nb, dtype=np.int64)
    owner = np.empty(sys.n_dof, dtype=np.int64)
    owner[:nb] = np.arange(nb)
    nbrs = [set() for _ in range(nb)]
    for el, lm in zip(net.elements, sys.conn):
        if isinstance(el, BuckConverter):
            owner[lm[0]] = el.node; w[el.node] += conv_weight
        elif isinstance(el, Line):
            owner[lm[0]] = el.a; w[el.a] += line_weight
            if el.a != el.b:
                nbrs[el.a].add(el.b); nbrs[el.b].add(el.a)
    xadj = np.zeros(nb + 1, dtype=np.int64)
    adj = []
    for i in range(nb):
        s = sorted(nbrs[i]); adj.extend(s); xadj[i + 1] = xadj[i] + len(s)
    return xadj, np.array(adj, dtype=np.int64), w, owner


def partition_metis(sys, nparts, conv_weight=4, seed=0):
    """METIS k-way partition of the bus graph -> rank of every global DOF."""
    import pymetis
    xadj, adjncy, w, owner = bus_graph(sys, conv_weight)
    if nparts == 1:
        return np.zeros(sys.n_dof, dtype=np.int64)
    opts = pymetis.Options(seed=seed)
    adjacency = pymetis.CSRAdjacency(xadj, adjncy)
    _, part_bus = pymetis.part_graph(nparts, adjacency, vweights=w.tolist(), options=opts)
    return np.asarray(part_bus, dtype=np.int64)[owner]


def partition_block(sys, nparts, order=None):
    """Naive: chop the bus list (in `order`) into equal contiguous blocks."""
    _, _, _, owner = bus_graph(sys)
    nb = sys.n_nodes
    order = np.arange(nb) if order is None else np.asarray(order)
    part_bus = np.empty(nb, dtype=np.int64)
    part_bus[order] = (np.arange(nb) * nparts) // nb
    return part_bus[owner]


def partition_one_element_per_rank(sys):
    """Anti-pattern baseline: every element is its own rank."""
    part = -np.ones(sys.n_dof, dtype=np.int64)
    for r, (el, lm) in enumerate(zip(sys.network.elements, sys.conn)):
        part[lm[:el.n_internal]] = r
        for n in lm[el.n_internal:]:
            if part[n] < 0:
                part[n] = r
    return part


@dataclass
class RankPlan:
    rank: int
    owned: np.ndarray
    ghosts: np.ndarray
    ghost_owner: np.ndarray
    A_loc: sp.csr_matrix


def distributed_plan(sys, part):
    """Build what an MPI code would build: per-rank local CSR + halo lists."""
    A = sys.A.tocsr()
    plans = []
    for r in range(int(part.max()) + 1):
        owned = np.nonzero(part == r)[0]
        sub = A[owned]
        cols = np.unique(sub.indices)
        ghosts = np.setdiff1d(cols, owned, assume_unique=True)
        local_cols = np.concatenate([owned, ghosts])
        g2l = -np.ones(sys.n_dof, dtype=np.int64)
        g2l[local_cols] = np.arange(local_cols.size)
        A_loc = sp.csr_matrix((sub.data, g2l[sub.indices], sub.indptr),
                              shape=(owned.size, local_cols.size))
        plans.append(RankPlan(r, owned, ghosts, part[ghosts], A_loc))
    return plans


def emulate_distributed_spmv(plans, x, n_dof):
    """Distributed y = A x, emulated rank by rank."""
    y = np.zeros(n_dof)
    for p in plans:
        xl = np.concatenate([x[p.owned], x[p.ghosts]])
        y[p.owned] = p.A_loc @ xl
    return y


def partition_stats(sys, part, conv_weight=4):
    """Load balance and communication metrics of a DOF partition."""
    plans = distributed_plan(sys, part)
    work = np.zeros(len(plans))
    is_conv = np.zeros(sys.n_dof, bool); is_conv[sys.conv_state] = True
    for p in plans:
        work[p.rank] = p.owned.size + (conv_weight - 1) * is_conv[p.owned].sum() + p.A_loc.nnz
    halo = np.array([p.ghosts.size for p in plans])
    neigh = np.array([np.unique(p.ghost_owner).size for p in plans])
    return dict(
        nparts=len(plans),
        imbalance=float(work.max() / work.mean()),
        halo_total=int(halo.sum()),
        halo_max=int(halo.max()),
        neighbours_max=int(neigh.max()),
        comm_to_comp=float(halo.sum() / work.sum()),
    )
