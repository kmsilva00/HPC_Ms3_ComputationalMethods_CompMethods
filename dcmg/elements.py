from __future__ import annotations

from dataclasses import dataclass, field
import numpy as np


@dataclass
class Element:

    n_internal: int = field(init=False, default=0)

    def nodes(self) -> list[int]:
        """Global node indices this element touches (its 'external' DOFs)."""
        raise NotImplementedError

    def local_stamp(self):
        """Return (m_e, A_e, B_e, b_e) in LOCAL numbering."""
        raise NotImplementedError


@dataclass
class BuckConverter(Element):

    node: int = 0
    E: float = 48.0
    L: float = 470e-6
    rL: float = 0.05
    name: str = ""

    def __post_init__(self):
        self.n_internal = 1

    def nodes(self):
        return [self.node]

    def local_stamp(self):
        m = np.array([self.L])
        A = np.array([[-self.rL, -1.0],
                      [+1.0,      0.0]])
        B = np.array([self.E, 0.0])
        b = np.zeros(2)
        return m, A, B, b


@dataclass
class Line(Element):

    a: int = 0
    b: int = 1
    R: float = 0.05
    L: float = 10e-6

    def __post_init__(self):
        self.n_internal = 1

    def nodes(self):
        return [self.a, self.b]

    def local_stamp(self):
        m = np.array([self.L])
        A = np.array([[-self.R, +1.0, -1.0],
                      [-1.0,     0.0,  0.0],
                      [+1.0,     0.0,  0.0]])
        return m, A, np.zeros(3), np.zeros(3)


@dataclass
class ResistiveLoad(Element):


    node: int = 0
    R: float = 10.0

    def nodes(self):
        return [self.node]

    def local_stamp(self):
        return np.zeros(0), np.array([[-1.0 / self.R]]), np.zeros(1), np.zeros(1)


@dataclass
class ConstantCurrentLoad(Element):


    node: int = 0
    I: float = 1.0

    def nodes(self):
        return [self.node]

    def local_stamp(self):
        return np.zeros(0), np.zeros((1, 1)), np.zeros(1), np.array([-self.I])


@dataclass
class ConstantPowerLoad(Element):
    node: int = 0
    P: float = 100.0
    v_min: float = 5.0

    def nodes(self):
        return [self.node]

    def local_stamp(self):
        return np.zeros(0), np.zeros((1, 1)), np.zeros(1), np.zeros(1)


def cpl_current(v, P, v_min):
    v = np.asarray(v)
    hi = v >= v_min
    return np.where(hi, P / np.where(hi, v, 1.0), P * v / v_min**2)


def cpl_dcurrent_dv(v, P, v_min):
    v = np.asarray(v)
    hi = v >= v_min
    return np.where(hi, -P / np.where(hi, v, 1.0) ** 2, P / v_min**2)
