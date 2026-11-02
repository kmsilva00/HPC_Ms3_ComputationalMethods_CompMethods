"""Validation suite: each test checks the simulator against an independent result."""
import numpy as np
import pytest
import scipy.linalg as sla

from dcmg import (Network, BuckConverter, Line, ResistiveLoad, ConstantPowerLoad,
                  OpenLoopPWM, SlidingModeDroop, SecondaryVoltageControl, simulate)
from dcmg.topologies import single_buck, parallel_bucks, meshed_grid
from dcmg.analysis import equilibrium, eigenvalues, buck_cpl_stability_margin
from dcmg.partition import (partition_metis, distributed_plan,
                            emulate_distributed_spmv)

L, C, rL, E = 470e-6, 470e-6, 0.05, 48.0


def test_assembled_equals_element_by_element():
    """Global sparse assembly == looping over element stamps (matrix-free)."""
    s = meshed_grid(8, 8, seed=3)[0].assemble()
    rng = np.random.default_rng(0)
    x = rng.uniform(10, 30, s.n_dof)
    u = rng.integers(0, 2, s.n_conv).astype(float)
    np.testing.assert_allclose(s.kirchhoff(x, u), s.kirchhoff_ebe(x, u), rtol=1e-12, atol=1e-12)


def test_lines_conserve_charge():
    """With only lines and capacitors, total current into all nodes is zero."""
    net = Network()
    for _ in range(5):
        net.add_node(1e-4)
    for a, b in [(0, 1), (1, 2), (2, 3), (3, 4), (4, 0), (0, 2)]:
        net.add(Line(a=a, b=b, R=0.1, L=1e-5))
    s = net.assemble()
    x = np.random.default_rng(1).normal(size=s.n_dof)
    assert abs((s.A @ x)[: s.n_nodes].sum()) < 1e-12


def test_buck_dc_gain():
    """Averaged buck with resistor: V = D E R / (R + rL)."""
    s = single_buck(R_load=10.0)[0].assemble()
    for D in (0.25, 0.5, 0.8):
        x = equilibrium(s, [D])
        assert x[0] == pytest.approx(D * E * 10 / 10.05, rel=1e-10)


def test_rk4_matches_matrix_exponential():
    """Switch held closed: linear RLC circuit, exact solution x(t) = expm(J t) x0 + ..."""
    s = single_buck(R_load=10.0)[0].assemble()
    T = 3e-3
    r = simulate(s, [OpenLoopPWM([0], 1.0)], T, mode="averaged", dt=1e-6)
    J = s.jacobian(np.zeros(2)).toarray()
    c = s.minv * (s.B @ np.ones(1))
    exact = np.linalg.solve(J, (sla.expm(J * T) - np.eye(2)) @ c)
    np.testing.assert_allclose(r.x[-1], exact, rtol=1e-8)


def test_pwm_time_events_exact():
    """20 kHz PWM for 1 ms -> exactly 40 switch transitions, at exact instants."""
    s = single_buck(R_load=10.0)[0].assemble()
    r = simulate(s, [OpenLoopPWM([0], 0.3, fs=20e3)], 1e-3, mode="hybrid", dt=1e-5)
    assert r.events_per_conv[0] == 40
    on = r.t[1:][np.diff(r.q[:, 0]) > 0.5]
    np.testing.assert_allclose(np.diff(on), 50e-6, rtol=1e-9)


def test_cpl_eigenvalues_match_closed_form():
    P = 150.0
    s = single_buck(P_cpl=P)[0].assemble()
    x = equilibrium(s, [0.5])
    lam = eigenvalues(s, x)
    margin = buck_cpl_stability_margin(L, C, rL, P, x[0])
    assert 2 * lam.real.mean() == pytest.approx(-margin, rel=1e-9)
    assert margin < 0 and lam.real.max() > 0


def test_cpl_growth_rate_matches_eigenvalue():
    """A small perturbation grows like exp(Re(lambda) t) in the averaged model."""
    s = single_buck(P_cpl=150.0)[0].assemble()
    xe = equilibrium(s, [0.5])
    sigma = eigenvalues(s, xe).real.max()
    x0 = xe.copy(); x0[0] += 1e-4
    r = simulate(s, [OpenLoopPWM([0], 0.5)], 6e-3, x0=x0, mode="averaged", dt=1e-6)
    dev = np.abs(r.x[:, 0] - xe[0])
    T_osc = 2 * np.pi / abs(eigenvalues(s, xe).imag.max())
    n = int(T_osc / 1e-6)
    a0, a1 = dev[:n].max(), dev[-n:].max()
    est = np.log(a1 / a0) / (r.t[-1] - T_osc)
    assert est == pytest.approx(sigma, rel=0.05)


def test_sliding_mode_stabilises_cpl():
    """Same CPL: open-loop PWM diverges, sliding mode regulates to V_ref."""
    s = single_buck(P_cpl=150.0)[0].assemble()
    xe = equilibrium(s, [0.5])
    x0 = xe.copy(); x0[0] += 0.05
    ol = simulate(s, [OpenLoopPWM([0], 0.5)], 10e-3, x0=x0, mode="hybrid", dt=2e-6)
    assert np.ptp(ol.x[ol.t > 8e-3, 0]) > 5.0
    smc = SlidingModeDroop([0], V_ref=24.0, lam=1.0, R_d=0.0, h=0.5)
    cl = simulate(s, [smc], 10e-3, x0=s.initial_state(0.0), mode="hybrid", dt=2e-6)
    v = cl.x[cl.t > 6e-3, 0]
    assert abs(v.mean() - 24.0) < 0.05 and np.ptp(v) < 0.1


def test_droop_current_sharing():
    """Equal (tiny) lines, droop 0.2 vs 0.4 ohm -> converter 0 supplies ~2x converter 1."""
    s = parallel_bucks(n_conv=2, R_line=(0.005, 0.005), P_cpl=150, R_load=20)[0].assemble()
    smc = SlidingModeDroop([0, 1], V_ref=24.0, lam=1.0, R_d=[0.2, 0.4], h=0.5)
    r = simulate(s, [smc], 20e-3, mode="averaged", dt=2e-6, x0=s.initial_state(0.0))
    i = r.x[-1, s.conv_state]
    assert i[0] / i[1] == pytest.approx(2.0, rel=0.03)


def test_three_integrators_agree():
    """Hybrid (event-located), sampled (ZOH) and averaged give the same mean behaviour."""
    s = parallel_bucks()[0].assemble()
    out = {}
    for mode, dt in [("hybrid", 2e-6), ("sampled", 0.5e-6), ("averaged", 2e-6)]:
        smc = SlidingModeDroop([0, 1, 2], V_ref=24.0, lam=1.0, R_d=0.25, h=0.5)
        r = simulate(s, [smc], 12e-3, mode=mode, dt=dt, x0=s.initial_state(0.0))
        w = r.t > 10e-3
        out[mode] = (r.x[w, 0].mean(), r.x[w][:, s.conv_state].mean(0))
    for m in ("sampled", "averaged"):
        assert out[m][0] == pytest.approx(out["hybrid"][0], abs=0.02)
        np.testing.assert_allclose(out[m][1], out["hybrid"][1], atol=0.05)


def test_secondary_control_restores_voltage():
    net, meta = parallel_bucks()
    s = net.assemble()
    smc = SlidingModeDroop([0, 1, 2], V_ref=24.0, lam=1.0, R_d=0.25, h=0.5)
    sec = SecondaryVoltageControl(smc, meta["bus"], V_nom=24.0, ki=100.0, period=100e-6)
    r = simulate(s, [smc], 60e-3, mode="averaged", dt=2e-6, x0=s.initial_state(0.0),
                 supervisors=[sec])
    assert r.x[-1, meta["bus"]] == pytest.approx(24.0, abs=0.02)


def test_distributed_spmv_and_locality():
    s = meshed_grid(20, 20, seed=1)[0].assemble()
    part = partition_metis(s, 8)
    plans = distributed_plan(s, part)
    x = np.random.default_rng(2).normal(size=s.n_dof)
    np.testing.assert_allclose(emulate_distributed_spmv(plans, x, s.n_dof), s.A @ x, atol=1e-12)
    assert np.all(part[s.conv_state] == part[s.conv_node])
