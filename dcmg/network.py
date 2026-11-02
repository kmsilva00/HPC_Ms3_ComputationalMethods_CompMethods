"""Network description + FEM-style global assembly."""
from __future__ import annotations

from dataclasses import dataclass
import numpy as np
import scipy.sparse as sp

from .elements import (Element, BuckConverter, Line, ConstantPowerLoad,
                       cpl_current, cpl_dcurrent_dv)


class Network:
    def __init__(self):
        self.node_C: list[float] = []
        self.node_names: list[str] = []
        self.elements: list[Element] = []

    def add_node(self, C: float, name: str = "") -> int:
        if C <= 0:
            raise ValueError("every node needs C > 0 (keeps M invertible: ODE, not DAE)")
        self.node_C.append(float(C))
        self.node_names.append(name or f"n{len(self.node_C) - 1}")
        return len(self.node_C) - 1

    def add(self, el: Element) -> Element:
        for n in el.nodes():
            if not (0 <= n < len(self.node_C)):
                raise IndexError(f"element touches unknown node {n}")
        self.elements.append(el)
        return el

    def connectivity(self):
        """Per element: array mapping local DOF -> global DOF (FEM 'LM' array)."""
        n_nodes = len(self.node_C)
        nxt = n_nodes
        conn = []
        for el in self.elements:
            internal = list(range(nxt, nxt + el.n_internal))
            nxt += el.n_internal
            conn.append(np.array(internal + el.nodes(), dtype=np.int64))
        return conn, nxt

    def assemble(self) -> "AssembledSystem":
        n_nodes = len(self.node_C)
        conn, n_dof = self.connectivity()
        convs = [k for k, el in enumerate(self.elements) if isinstance(el, BuckConverter)]
        n_u = len(convs)
        conv_col = {k: j for j, k in enumerate(convs)}

        m = np.zeros(n_dof)
        m[:n_nodes] = self.node_C
        b0 = np.zeros(n_dof)
        rows, cols, vals = [], [], []
        brow, bcol, bval = [], [], []
        cpl_node, cpl_P, cpl_vmin = [], [], []

        for k, (el, lm) in enumerate(zip(self.elements, conn)):
            m_e, A_e, B_e, b_e = el.local_stamp()
            ni = el.n_internal
            m[lm[:ni]] += m_e
            ii, jj = np.nonzero(A_e)
            rows.append(lm[ii]); cols.append(lm[jj]); vals.append(A_e[ii, jj])
            b0[lm] += b_e
            nz = np.nonzero(B_e)[0]
            if nz.size:
                brow.append(lm[nz]); bcol.append(np.full(nz.size, conv_col[k])); bval.append(B_e[nz])
            if isinstance(el, ConstantPowerLoad):
                cpl_node.append(el.node); cpl_P.append(el.P); cpl_vmin.append(el.v_min)

        cat = lambda L, dt=float: np.concatenate(L) if L else np.zeros(0, dt)
        A = sp.coo_matrix((cat(vals), (cat(rows, np.int64), cat(cols, np.int64))),
                          shape=(n_dof, n_dof)).tocsr()
        B = sp.coo_matrix((cat(bval), (cat(brow, np.int64), cat(bcol, np.int64))),
                          shape=(n_dof, n_u)).tocsr()

        conv_state = np.array([conn[k][0] for k in convs], dtype=np.int64)
        conv_node = np.array([self.elements[k].node for k in convs], dtype=np.int64)
        line_ids = [k for k, el in enumerate(self.elements) if isinstance(el, Line)]

        return AssembledSystem(
            n_nodes=n_nodes, n_dof=n_dof, m=m, A=A, B=B, b0=b0,
            cpl_node=np.array(cpl_node, dtype=np.int64),
            cpl_P=np.array(cpl_P, float), cpl_vmin=np.array(cpl_vmin, float),
            conv_state=conv_state, conv_node=conv_node,
            conv_E=np.array([self.elements[k].E for k in convs]),
            line_state=np.array([conn[k][0] for k in line_ids], dtype=np.int64),
            network=self, conn=conn,
        )


@dataclass
class AssembledSystem:
    n_nodes: int
    n_dof: int
    m: np.ndarray
    A: sp.csr_matrix
    B: sp.csr_matrix
    b0: np.ndarray
    cpl_node: np.ndarray
    cpl_P: np.ndarray
    cpl_vmin: np.ndarray
    conv_state: np.ndarray
    conv_node: np.ndarray
    conv_E: np.ndarray
    line_state: np.ndarray
    network: Network
    conn: list

    def __post_init__(self):
        self.minv = 1.0 / self.m

    @property
    def n_conv(self):
        return self.conv_state.size

    def f_nl(self, x):
        """Nonlinear forcing vector (CPL currents leave their nodes)."""
        f = np.zeros_like(x)
        if self.cpl_node.size:
            np.subtract.at(f, self.cpl_node, cpl_current(x[self.cpl_node], self.cpl_P, self.cpl_vmin))
        return f

    def kirchhoff(self, x, u):
        """M dx/dt  (i.e."""
        return self.A @ x + self.B @ u + self.b0 + self.f_nl(x)

    def rhs(self, x, u):
        return self.minv * self.kirchhoff(x, u)

    def jacobian(self, x):
        """d(rhs)/dx (u enters affinely, so J does not depend on u)."""
        J = self.A.tolil(copy=True)
        if self.cpl_node.size:
            d = cpl_dcurrent_dv(x[self.cpl_node], self.cpl_P, self.cpl_vmin)
            for n, dn in zip(self.cpl_node, d):
                J[n, n] -= dn
        return sp.diags(self.minv) @ J.tocsr()

    def cap_current(self, x):
        """Capacitor current C dv/dt at every converter's output node."""
        k = self.A @ x + self.b0 + self.f_nl(x)
        return k[self.conv_node]

    def output_current(self, x):
        """Current each converter pushes into the rest of the grid: i_L - i_C."""
        share = np.bincount(self.conv_node, minlength=self.n_nodes)[self.conv_node]
        return x[self.conv_state] - self.cap_current(x) / share

    def kirchhoff_ebe(self, x, u):
        """Matrix-free `kirchhoff`: element-by-element gather, multiply, scatter."""
        out = self.b0.copy() + self.f_nl(x)
        j = 0
        for el, lm in zip(self.network.elements, self.conn):
            _, A_e, B_e, _ = el.local_stamp()
            loc = A_e @ x[lm]
            if np.any(B_e):
                loc = loc + B_e * u[j]
                j += 1
            np.add.at(out, lm, loc)
        return out

    def initial_state(self, v0: float | np.ndarray = 0.0):
        x = np.zeros(self.n_dof)
        x[: self.n_nodes] = v0
        return x
