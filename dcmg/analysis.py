"""Equilibria and small-signal stability of the averaged model."""
from __future__ import annotations

import numpy as np
import scipy.sparse as sp
import scipy.sparse.linalg as spla

from .elements import cpl_dcurrent_dv


def equilibrium(sys, u, x0=None, v_guess=None, tol=1e-10, maxit=50):
    """Newton solve of  A x + B u + b0 + f(x) = 0  for a constant duty vector u."""
    u = np.broadcast_to(np.asarray(u, float), (sys.n_conv,))
    if x0 is None:
        x0 = sys.initial_state(v_guess if v_guess is not None else float(np.mean(u * sys.conv_E)))
    x = x0.copy()
    for _ in range(maxit):
        F = sys.kirchhoff(x, u)
        J = sys.A.tolil(copy=True)
        if sys.cpl_node.size:
            d = cpl_dcurrent_dv(x[sys.cpl_node], sys.cpl_P, sys.cpl_vmin)
            for n, dn in zip(sys.cpl_node, d):
                J[n, n] -= dn
        dx = spla.spsolve(J.tocsc(), -F)
        x += dx
        if np.linalg.norm(dx) < tol * (1 + np.linalg.norm(x)):
            return x
    raise RuntimeError("Newton did not converge (maybe no equilibrium exists: CPL too large)")


def eigenvalues(sys, x):
    """Eigenvalues of the linearised averaged model around x (dense; small systems)."""
    return np.linalg.eigvals(sys.jacobian(x).toarray())


def buck_cpl_stability_margin(L, C, rL, P, V, R=np.inf):
    """Closed-form check for ONE open-loop buck feeding R || CPL."""
    G = (0.0 if np.isinf(R) else 1.0 / R) - P / V**2
    return rL / L + G / C
