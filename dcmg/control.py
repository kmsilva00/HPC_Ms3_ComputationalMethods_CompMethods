from __future__ import annotations

import numpy as np


class Controller:
    def __init__(self, idx):
        self.idx = np.atleast_1d(np.asarray(idx, dtype=np.int64))

    def guard(self, t, x, q, sys):
        return None

    def next_event_time(self, t, q):
        return None

    def jump(self, t, x, q, mask, sys):
        raise NotImplementedError

    def initial_q(self, t, x, sys):
        raise NotImplementedError

    def sample(self, t, x, q, sys):
        raise NotImplementedError

    def duty(self, t, x, u, sys, dt=None):
        raise NotImplementedError


class OpenLoopPWM(Controller):

    def __init__(self, idx, duty, fs=20e3, phase=0.0):
        super().__init__(idx)
        n = self.idx.size
        self.D = np.broadcast_to(np.asarray(duty, float), (n,)).copy()
        self.T = 1.0 / fs
        self.phase = np.broadcast_to(np.asarray(phase, float), (n,)).copy()

    def _desired(self, t):
        frac = np.mod(t / self.T + self.phase, 1.0)
        return (frac < self.D).astype(float)

    def initial_q(self, t, x, sys):
        return self._desired(t + 1e-9 * self.T)

    def next_event_time(self, t, q):
        tp = t / self.T + self.phase
        n = np.floor(tp)
        cands = np.stack([n + self.D, n + 1.0, n + 1.0 + self.D, n + 2.0])
        cands = (cands - self.phase) * self.T
        cands = np.where(cands > t + 1e-9 * self.T, cands, np.inf)
        return cands.min(axis=0)

    def jump(self, t, x, q, mask, sys):
        q = q.copy()
        q[mask] = self._desired(t + 1e-9 * self.T)[mask]
        return q

    def sample(self, t, x, q, sys):
        return self._desired(t)

    def duty(self, t, x, u, sys, dt=None):
        return self.D.copy()


class SlidingModeDroop(Controller):

    def __init__(self, idx, V_ref=24.0, lam=1.0, R_d=0.2, h=0.5, tau_reach=None):
        super().__init__(idx)
        n = self.idx.size
        bc = lambda a: np.broadcast_to(np.asarray(a, float), (n,)).copy()
        self.V_ref = bc(V_ref)
        self.lam = bc(lam)
        self.R_d = bc(R_d)
        self.h = bc(h)
        self.tau_reach = tau_reach

    def sigma(self, x, sys):
        v = x[sys.conv_node[self.idx]]
        iC = sys.cap_current(x)[self.idx]
        iout = sys.output_current(x)[self.idx]
        return self.lam * (v - (self.V_ref - self.R_d * iout)) + iC

    def initial_q(self, t, x, sys):
        return (self.sigma(x, sys) < 0).astype(float)

    def guard(self, t, x, q, sys):
        s = self.sigma(x, sys)
        return np.where(q > 0.5, s - self.h, -s - self.h)

    def jump(self, t, x, q, mask, sys):
        q = q.copy()
        q[mask] = 1.0 - q[mask]
        return q

    def sample(self, t, x, q, sys):
        s = self.sigma(x, sys)
        q = q.copy()
        q[s <= -self.h] = 1.0
        q[s >= self.h] = 0.0
        return q

    def duty(self, t, x, u, sys, dt=1e-6):
        """Equivalent control u_eq in [0, 1] (Utkin)."""
        tau = self.tau_reach or 10 * dt
        eps = 1e-9
        u0 = u.copy(); u0[self.idx] = 0.0
        u1 = u.copy(); u1[self.idx] = 1.0
        s = self.sigma(x, sys)
        a = (self.sigma(x + eps * sys.rhs(x, u0), sys) - s) / eps
        b = (self.sigma(x + eps * sys.rhs(x, u1), sys) - s) / eps - a
        return np.clip((-s / tau - a) / b, 0.0, 1.0)


class SecondaryVoltageControl:

    def __init__(self, smc: SlidingModeDroop, bus_nodes, V_nom=24.0, ki=30.0,
                 period=100e-6, delta_max=5.0, t_start=0.0):
        self.smc = smc
        self.bus_nodes = np.atleast_1d(bus_nodes)
        self.V_nom = V_nom
        self.ki = ki
        self.period = period
        self.delta = 0.0
        self.delta_max = delta_max
        self.t_start = t_start
        self.base = smc.V_ref.copy()

    def update(self, t, x, sys):
        if t < self.t_start:
            return
        err = self.V_nom - float(np.mean(x[self.bus_nodes]))
        self.delta = float(np.clip(self.delta + self.ki * self.period * err,
                                   -self.delta_max, self.delta_max))
        self.smc.V_ref[:] = self.base + self.delta
