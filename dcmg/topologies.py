
from __future__ import annotations

import numpy as np

from .network import Network
from .elements import BuckConverter, Line, ResistiveLoad, ConstantPowerLoad

DEFAULTS = dict(E=48.0, L=470e-6, rL=0.05, C=470e-6)


def single_buck(P_cpl=0.0, R_load=np.inf, E=48.0, L=470e-6, rL=0.05, C=470e-6):
    """One buck converter, its output capacitor, and a load (R and/or CPL)."""
    net = Network()
    n = net.add_node(C, "out")
    net.add(BuckConverter(node=n, E=E, L=L, rL=rL, name="buck0"))
    if np.isfinite(R_load):
        net.add(ResistiveLoad(node=n, R=R_load))
    if P_cpl > 0:
        net.add(ConstantPowerLoad(node=n, P=P_cpl))
    return net, dict(out=n)


def parallel_bucks(n_conv=3, P_cpl=200.0, R_load=20.0, R_line=(0.05, 0.10, 0.15),
                   L_line=10e-6, C_conv=470e-6, C_bus=1000e-6, E=48.0, L=470e-6, rL=0.05):
    """`n_conv` converters connected through lines to one bus feeding R || CPL."""
    R_line = np.broadcast_to(np.asarray(R_line, float), (n_conv,))
    net = Network()
    bus = net.add_node(C_bus, "bus")
    conv_nodes = []
    for k in range(n_conv):
        nk = net.add_node(C_conv, f"c{k}")
        conv_nodes.append(nk)
        net.add(BuckConverter(node=nk, E=E, L=L, rL=rL, name=f"buck{k}"))
        net.add(Line(a=nk, b=bus, R=R_line[k], L=L_line))
    if np.isfinite(R_load):
        net.add(ResistiveLoad(node=bus, R=R_load))
    cpl = net.add(ConstantPowerLoad(node=bus, P=P_cpl)) if P_cpl > 0 else None
    return net, dict(bus=bus, conv_nodes=conv_nodes, cpl=cpl)


def meshed_grid(nx=10, ny=10, conv_fraction=0.3, P_cpl=(20.0, 80.0), R_line=(0.02, 0.08),
                L_line=5e-6, C_node=200e-6, E=48.0, L=470e-6, rL=0.05, seed=0,
                drop_fraction=0.1):
    rng = np.random.default_rng(seed)
    net = Network()
    ids = np.zeros((nx, ny), dtype=int)
    for i in range(nx):
        for j in range(ny):
            ids[i, j] = net.add_node(C_node, f"b{i}_{j}")
    edges = []
    for i in range(nx):
        for j in range(ny):
            if i + 1 < nx: edges.append((ids[i, j], ids[i + 1, j]))
            if j + 1 < ny: edges.append((ids[i, j], ids[i, j + 1]))
    rng.shuffle(edges)
    parent = list(range(nx * ny))
    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]; a = parent[a]
        return a
    keep = []
    rest = []
    for a, b in edges:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[ra] = rb; keep.append((a, b))
        else:
            rest.append((a, b))
    n_extra = int(round((1 - drop_fraction) * len(edges))) - len(keep)
    keep += rest[:max(n_extra, 0)]
    for a, b in keep:
        net.add(Line(a=a, b=b, R=rng.uniform(*R_line), L=L_line))

    n_bus = nx * ny
    is_src = np.zeros(n_bus, bool)
    is_src[rng.choice(n_bus, size=max(1, int(conv_fraction * n_bus)), replace=False)] = True
    for b in range(n_bus):
        if is_src[b]:
            net.add(BuckConverter(node=b, E=E, L=L, rL=rL))
        else:
            net.add(ConstantPowerLoad(node=b, P=rng.uniform(*P_cpl)))
    xy = np.array([(i, j) for i in range(nx) for j in range(ny)], float)
    return net, dict(ids=ids, is_src=is_src, xy=xy, edges=keep)
