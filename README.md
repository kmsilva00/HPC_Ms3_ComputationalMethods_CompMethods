# dcmg-sim

Reference simulator for switched DC microgrids: DC/DC buck converters with switching
controllers, lines, and constant-power loads. It is the validated Python baseline for the
project's later C++/MPI implementation.

Reda · Kevin · Leonint — Computational Methods project

## Quick start

```bash
pip install -r requirements.txt
python -m pytest -q tests            # 12 tests, about 40 s
cd demos && python demo2_cpl_instability.py   # figures are written to ../figures
```

## Model

```
M dx/dt = A x + B u + b0 + f(x),     u_k ∈ {0, 1}
```

| Symbol | Meaning |
|---|---|
| `x` | capacitor (node) voltages and inductor currents |
| `M` | diagonal: capacitances and inductances |
| `A` | sparse matrix assembled from per-element stamps |
| `B u` | source voltage injected by each converter's switch |
| `b0`, `f(x)` | constant-current loads and constant-power loads `-P/v` |

The switches make this a hybrid system: continuous flow between switch flips, discrete
jumps when a controller's guard crosses zero.

## Package

| Module | Content |
|---|---|
| `dcmg/elements.py` | buck converter, line, resistive / constant-current / constant-power loads, each with a local stamp |
| `dcmg/network.py` | numbering, sparse assembly, right-hand side, matrix-free check |
| `dcmg/control.py` | open-loop PWM, sliding-mode control with droop, secondary voltage control |
| `dcmg/simulate.py` | integrators: `hybrid` (RK4 + event location), `sampled` (fixed step), `averaged` |
| `dcmg/analysis.py` | equilibria (Newton), eigenvalues, constant-power-load stability margin |
| `dcmg/topologies.py` | single converter, parallel converters on one bus, meshed grids |
| `dcmg/partition.py` | METIS partitioning of the bus graph, ghost lists, distributed SpMV emulation |

## Example

```python
from dcmg import SlidingModeDroop, simulate
from dcmg.topologies import parallel_bucks

net, meta = parallel_bucks(3)
sys = net.assemble()
smc = SlidingModeDroop([0, 1, 2], V_ref=24, R_d=0.25, h=0.5)
res = simulate(sys, [smc], 80e-3, mode="hybrid")   # res.t, res.x, res.q
```

## Demos

| Script | Result |
|---|---|
| `demo1_single_buck.py` | switched vs. averaged converter |
| `demo2_cpl_instability.py` | constant-power-load instability without control; sliding mode stabilises it |
| `demo3_parallel_hierarchy.py` | three converters: droop sharing, load step, secondary voltage restoration |
| `demo4_partitioning.py` | METIS vs. naive partitions of a 40 × 40 grid |
| `demo5_integrator_cost.py` | cost and accuracy of exact vs. fixed-step switching |
| `demo6_event_vs_overshoot.py` | locating the switching instant vs. stepping past it |

## Tests

Seven tests compare the simulation with hand-derived results: converter output voltage,
exact solution of a linear RLC circuit, constant-power-load eigenvalues and growth rate,
sliding-mode regulation, droop current sharing, secondary voltage restoration. Five check
consistency: assembly vs. element-by-element product, charge conservation, exact PWM
switching instants, agreement of the three integrators, distributed vs. serial SpMV.

## References

- R. Goebel, R. G. Sanfelice, A. R. Teel, "Hybrid dynamical systems", 2009.
- V. Utkin, sliding mode control of DC/DC converters, 2013.
- M. Srinivasan, A. Kwasinski, control of parallel converters with constant-power loads, 2020.
- Mosayebi, Gheisarnejad, Khooban, intelligent sliding mode control of DC microgrids, 2022.
- K.-J. Bathe, MIT RES.2-002 *Finite Element Procedures*, lecture 1.
