
from __future__ import annotations

import time
from dataclasses import dataclass, field
import numpy as np


@dataclass
class Result:
    t: np.ndarray
    x: np.ndarray
    q: np.ndarray
    record_idx: np.ndarray
    n_steps: int = 0
    n_events: int = 0
    events_per_conv: np.ndarray = None
    wall: float = 0.0
    extra: dict = field(default_factory=dict)

    def col(self, dof):
        j = np.searchsorted(self.record_idx, dof)
        assert self.record_idx[j] == dof, "DOF was not recorded"
        return self.x[:, j]


def _rk4(sys, x, u, h):
    k1 = sys.rhs(x, u)
    k2 = sys.rhs(x + 0.5 * h * k1, u)
    k3 = sys.rhs(x + 0.5 * h * k2, u)
    k4 = sys.rhs(x + h * k3, u)
    return x + (h / 6.0) * (k1 + 2 * k2 + 2 * k3 + k4)


class _Bank:
    """Glue: concatenates guards / time events of all controllers."""

    def __init__(self, sys, controllers):
        self.sys, self.ctrls = sys, controllers
        covered = np.concatenate([c.idx for c in controllers]) if controllers else np.zeros(0, int)
        if sorted(covered.tolist()) != list(range(sys.n_conv)):
            raise ValueError("every converter must be driven by exactly one controller")

    def guards(self, t, x, u):
        parts, owners = [], []
        for ci, c in enumerate(self.ctrls):
            g = c.guard(t, x, u[c.idx], self.sys)
            if g is not None:
                parts.append(g); owners.append(np.full(g.size, ci))
        if not parts:
            return np.zeros(0), np.zeros(0, int), None
        return np.concatenate(parts), np.concatenate(owners), None

    def jump_state_events(self, t, x, u):
        """Apply jump map to every controller whose guard is >= 0."""
        n = 0
        for c in self.ctrls:
            g = c.guard(t, x, u[c.idx], self.sys)
            if g is None:
                continue
            mask = g >= 0
            if mask.any():
                u[c.idx] = c.jump(t, x, u[c.idx], mask, self.sys)
                self.ev[c.idx[mask]] += 1
                n += int(mask.sum())
        return n

    def next_time_event(self, t, u):
        tn = np.inf
        for c in self.ctrls:
            te = c.next_event_time(t, u[c.idx])
            if te is not None and te.size:
                tn = min(tn, float(te.min()))
        return tn

    def jump_time_events(self, t, x, u, tol):
        n = 0
        for c in self.ctrls:
            te = c.next_event_time(t - tol, u[c.idx])
            if te is None:
                continue
            mask = te <= t + tol
            if mask.any():
                new = c.jump(t, x, u[c.idx], mask, self.sys)
                changed = new != u[c.idx]
                u[c.idx] = new
                self.ev[c.idx[changed]] += 1
                n += int(changed.sum())
        return n


def simulate(sys, controllers, t_end, x0=None, mode="hybrid", dt=1e-6,
             supervisors=(), actions=(), record_dt=None, record_idx=None,
             max_events_per_step=50):
    """Integrate the closed-loop microgrid."""
    x = sys.initial_state() if x0 is None else np.array(x0, float)
    bank = _Bank(sys, controllers)
    bank.ev = np.zeros(sys.n_conv, dtype=np.int64)
    u = np.zeros(sys.n_conv)
    for c in controllers:
        u[c.idx] = c.initial_q(0.0, x, sys)
    if mode == "averaged":
        for c in controllers:
            u[c.idx] = c.duty(0.0, x, u, sys, dt=dt)

    rec_idx = np.arange(sys.n_dof) if record_idx is None else np.sort(np.asarray(record_idx))
    T, X, Q = [0.0], [x[rec_idx].copy()], [u.copy()]
    next_rec = record_dt or 0.0

    sup_next = [s.period for s in supervisors]
    acts = sorted(actions, key=lambda a: a[0])
    ai = 0
    t = 0.0
    n_steps = n_events = 0
    tol_t = 1e-12
    wall0 = time.perf_counter()

    while t < t_end - tol_t:
        t_lim = min(t + dt, t_end)
        if sup_next:
            t_lim = min(t_lim, min(sup_next))
        if ai < len(acts):
            t_lim = min(t_lim, acts[ai][0])

        if mode == "hybrid":
            t_lim = min(t_lim, bank.next_time_event(t, u))
            k = 0
            while bank.jump_state_events(t, x, u):
                k += 1
                if k > max_events_per_step:
                    raise RuntimeError(f"Zeno-like chattering at t={t}")
            n_events += k
            h = t_lim - t
            g0, _, _ = bank.guards(t, x, u)
            x1 = _rk4(sys, x, u, h)
            g1, _, _ = bank.guards(t_lim, x1, u)
            if g1.size and (g1 >= 0).any():
                cr = np.nonzero(g1 >= 0)[0]
                theta = g0[cr] / (g0[cr] - g1[cr])
                j = cr[np.argmin(theta)]
                lo, hi, glo, ghi = 0.0, 1.0, g0[j], g1[j]
                side = 0
                for _ in range(8):
                    th = lo + (hi - lo) * glo / (glo - ghi)
                    gt = bank.guards(t + th * h, _rk4(sys, x, u, th * h), u)[0][j]
                    if gt >= 0:
                        hi, ghi = th, gt
                        if side == +1: glo *= 0.5
                        side = +1
                    else:
                        lo, glo = th, gt
                        if side == -1: ghi *= 0.5
                        side = -1
                    if (hi - lo) * h < 1e-12:
                        break
                h = hi * h
                x = _rk4(sys, x, u, h)
                t = t + h
                n_events += bank.jump_state_events(t, x, u)
            else:
                x, t = x1, t_lim
            n_events += bank.jump_time_events(t, x, u, tol_t)

        else:
            h = t_lim - t
            if mode == "sampled":
                for c in controllers:
                    new = c.sample(t, x, u[c.idx], sys)
                    bank.ev[c.idx[new != u[c.idx]]] += 1
                    u[c.idx] = new
            elif mode == "averaged":
                for c in controllers:
                    u[c.idx] = c.duty(t, x, u, sys, dt=dt)
            else:
                raise ValueError(mode)
            x = _rk4(sys, x, u, h)
            t = t_lim

        n_steps += 1
        for i, s in enumerate(supervisors):
            if t >= sup_next[i] - tol_t:
                s.update(t, x, sys)
                sup_next[i] += s.period
        while ai < len(acts) and t >= acts[ai][0] - tol_t:
            acts[ai][1](sys)
            ai += 1
        if record_dt is None or t >= next_rec - tol_t:
            T.append(t); X.append(x[rec_idx].copy()); Q.append(u.copy())
            if record_dt:
                next_rec += record_dt

    if mode != "hybrid":
        n_events = int(bank.ev.sum())
    return Result(t=np.array(T), x=np.array(X), q=np.array(Q), record_idx=rec_idx,
                  n_steps=n_steps, n_events=n_events, events_per_conv=bank.ev.copy(),
                  wall=time.perf_counter() - wall0)
