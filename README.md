# dcmg-sim

ref simulator for switched DC microgrids: DC/DC buck converters with switching
controllers, lines, and constant-power loads. If validet than we will proceed to the port C++/MPI implementation.

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

## References

- R. Goebel, R. G. Sanfelice, A. R. Teel, "Hybrid dynamical systems", 2009.
- V. Utkin, sliding mode control of DC/DC converters, 2013.
- M. Srinivasan, A. Kwasinski, control of parallel converters with constant-power loads, 2020.
- Mosayebi, Gheisarnejad, Khooban, intelligent sliding mode control of DC microgrids, 2022.
- K.-J. Bathe, MIT RES.2-002 *Finite Element Procedures*, lecture 1.
