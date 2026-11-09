#!/usr/bin/env python3
"""Generate the KiCad 9 schematic for the Joust v2 wiring (docs/wiring/kicad/).

Pins come from firmware/include/config.h (source of truth). Once generated, edit the
schematic in KiCad; re-running overwrites it (refuses without --force).
Builder adapted from the WLED-Controller project's tools/gen_schematic.py.
"""
from __future__ import annotations

import argparse
import math
import os
import sys
import uuid

sys.path.insert(0, os.path.dirname(__file__))
from kicad_sexpr import Str, dump, find, load_lib_symbol, symbol_pins  # noqa: E402

KLIB = "/usr/share/kicad/symbols/"
OUT = os.path.join(os.path.dirname(__file__), "..", "kicad")
PROJECT = "joust-v2"
LIBNAME = "Joust"
ROOT_UUID = "4a0c57e2-6b1d-4f3e-9d2a-0c0ffee0j001".replace("j", "a")


def uid():
    return Str(str(uuid.uuid4()))


def r(v):
    return round(v, 4)


# ------------------------------------------------------------------ project block symbols
def F(size=1.27):
    return ["effects", ["font", ["size", size, size]]]


def block(name, title, left, right, width=20.32, desc="", top_pins=(), bottom_pins=()):
    """Rectangle with passive pins. left/right: list of (number, name) top to bottom; None = gap."""
    n = max(len(left), len(right))
    h = 2.54 * (n + 1)
    top = top_y = h / 2
    pins = []
    for side, lst in (("L", left), ("R", right)):
        for i, p in enumerate(lst):
            if p is None:
                continue
            num, nm = p
            y = r(top - 2.54 * (i + 1))
            x = -width / 2 - 2.54 if side == "L" else width / 2 + 2.54
            ang = 0 if side == "L" else 180
            pins.append(["pin", "passive", "line", ["at", r(x), y, ang], ["length", 2.54],
                         ["name", Str(nm), F()], ["number", Str(num), F()]])
    for side, lst in (("T", top_pins), ("B", bottom_pins)):
        for i, nm in enumerate(lst):
            x = r(-2.54 * (len(lst) - 1) + 5.08 * i)
            y = r(top_y + 2.54) if side == "T" else r(-top_y - 2.54)
            pins.append(["pin", "passive", "line", ["at", x, y, 270 if side == "T" else 90], ["length", 2.54],
                         ["name", Str(nm), F()], ["number", Str(nm), F()]])
    body = ["symbol", Str(f"{name}_0_1"),
            ["rectangle", ["start", r(-width / 2), r(top)], ["end", r(width / 2), r(-top)],
             ["stroke", ["width", 0.254], ["type", "default"]], ["fill", ["type", "background"]]]]
    return ["symbol", Str(name), ["pin_numbers", ["hide", "yes"]], ["pin_names", ["offset", 1.016]],
            ["exclude_from_sim", "no"], ["in_bom", "yes"], ["on_board", "yes"],
            ["property", Str("Reference"), Str("U"), ["at", 0, r(top + 1.27), 0], F()],
            ["property", Str("Value"), Str(title), ["at", 0, r(-top - 1.27), 0], F()],
            ["property", Str("Footprint"), Str(""), ["at", 0, 0, 0], ["effects", ["font", ["size", 1.27, 1.27]], ["hide", "yes"]]],
            ["property", Str("Datasheet"), Str(""), ["at", 0, 0, 0], ["effects", ["font", ["size", 1.27, 1.27]], ["hide", "yes"]]],
            ["property", Str("Description"), Str(desc), ["at", 0, 0, 0], ["effects", ["font", ["size", 1.27, 1.27]], ["hide", "yes"]]],
            body, ["symbol", Str(f"{name}_1_1"), *pins], ["embedded_fonts", "no"]]


def P(*names):
    return [(n, n) if n else None for n in names]


MEGA_LEFT = P("D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10", "D11", "D12", None,
              "A2", "A3", "A10", "A11", "D23", "D24", None, "A9", "A12")
MEGA_RIGHT = P("D13", "D22", None, "A0", "A1", None, "D44", "D45", None,
               "D18", "D19", None, "D20", "D21", None, "USB")

SYMBOLS = {
    "Arduino_Mega2560": lambda: block("Arduino_Mega2560", "Arduino Mega 2560 R3", MEGA_LEFT, MEGA_RIGHT, 20.32,
                                      "Only the pins Joust v2 uses. Pins: firmware/include/config.h", ("5V",), ("GND",)),
    "MotorShield_V2": lambda: block("MotorShield_V2", "Adafruit Motor Shield V2",
                                    P("SDA", "SCL", None, "VMOT+"),
                                    P("M1A", "M1B", None, "M2A", "M2B", None, "M3A", "M3B", None, "M4A", "M4B"),
                                    22.86, "PCA9685 + 2x TB6612, I2C 0x60, stacked on the Mega", ("5V",), ("GND", "VMOT-")),
    "DRV8871_Module": lambda: block("DRV8871_Module", "Adafruit DRV8871 (3190)",
                                    P("IN1", "IN2"), P("VM", None, "OUT1", "OUT2"), 17.78,
                                    "Brushed DC driver breakout, VM 6.5-45 V, ~2.1 A limit (30k ILIM)", (), ("GND", "PGND")),
    "Relay_Module_1ch": lambda: block("Relay_Module_1ch", "1-ch relay module",
                                      P("IN"), P("COM", "NO", "NC"), 15.24,
                                      "5 V relay module, contacts > 3 A at 12 V DC", ("VCC",), ("GND",)),
    "Relay_Module_2ch": lambda: block("Relay_Module_2ch", "2-ch relay board (chain, today)",
                                      P("IN1", "IN2"), P("V+", None, "MOT_A", "MOT_B"), 20.32,
                                      "Existing v1 chain relays; internal contact wiring TBD", ("VCC",), ("GND", "V-")),
    "PSU_12V": lambda: block("PSU_12V", "12 V PSU", [], P("+V"), 10.16, "Single 12 V supply, rating TBD", (), ("-V",)),
    "ESP32_Link": lambda: block("ESP32_Link", "ESP32 (future, S1)", P("RX", "TX"), [], 15.24,
                                "Future ESP32 bridge on Serial1 via level shifter", (), ("GND",)),
    "LevelShifter": lambda: block("LevelShifter", "Level shifter 5V<->3V3 (future)",
                                  P("HV1", "HV2"), P("LV1", "LV2"), 17.78,
                                  "Bidirectional MOSFET level shifter, e.g. BSS138 type", ("HV", "LV"), ("GND",)),
}


# ------------------------------------------------------------------ schematic builder
class Placed:
    def __init__(self, x, y, rot, pins):
        self.x, self.y, self.rot, self._pins = x, y, rot, pins

    def pin(self, num):
        px, py, _, _ = self._pins[str(num)]
        a = math.radians(self.rot)
        rx = px * math.cos(a) - py * math.sin(a)
        ry = px * math.sin(a) + py * math.cos(a)
        return (r(self.x + rx), r(self.y - ry))


class Sch:
    def __init__(self):
        self.libs, self.pins, self.items, self.placed = {}, {}, [], []
        self.inst_path = "/" + ROOT_UUID

    def lib(self, lib_id):
        if lib_id in self.libs:
            return
        lib, name = lib_id.split(":")
        sym = SYMBOLS[name]() if lib == LIBNAME else load_lib_symbol(KLIB + lib + ".kicad_sym", name)
        self.pins[lib_id] = symbol_pins(sym)
        sym = list(sym)
        sym[1] = Str(lib_id)
        self.libs[lib_id] = sym

    def place(self, lib_id, ref, value, x, y, rot=0, dnp=False, ref_off=None, val_off=None,
              hide_value=False, fields=None, in_bom=True):
        self.lib(lib_id)
        pins = self.pins[lib_id]
        is_power = ref.startswith("#")
        r_off = ref_off or (2.54, -1.27)
        v_off = val_off or (2.54, 1.27)
        fa = rot if rot in (90, 270) else 0
        props = [
            ["property", Str("Reference"), Str(ref), ["at", r(x + r_off[0]), r(y + r_off[1]), fa],
             ["effects", ["font", ["size", 1.27, 1.27]], ["justify", "left"]] + ([["hide", "yes"]] if is_power else [])],
            ["property", Str("Value"), Str(value), ["at", r(x + v_off[0]), r(y + v_off[1]), fa],
             ["effects", ["font", ["size", 1.27, 1.27]], ["justify", "left"]] + ([["hide", "yes"]] if hide_value else [])],
            ["property", Str("Footprint"), Str(""), ["at", x, y, 0], ["effects", ["font", ["size", 1.27, 1.27]], ["hide", "yes"]]],
            ["property", Str("Datasheet"), Str(""), ["at", x, y, 0], ["effects", ["font", ["size", 1.27, 1.27]], ["hide", "yes"]]],
        ]
        for k, v in (fields or {}).items():
            props.append(["property", Str(k), Str(v), ["at", x, y, 0], ["effects", ["font", ["size", 1.27, 1.27]], ["hide", "yes"]]])
        node = ["symbol", ["lib_id", Str(lib_id)], ["at", x, y, rot], ["unit", 1],
                ["exclude_from_sim", "no"], ["in_bom", "no" if (is_power or not in_bom) else "yes"],
                ["on_board", "no" if is_power else "yes"], ["dnp", "yes" if dnp else "no"],
                ["uuid", uid()], *props]
        for num in pins:
            node.append(["pin", Str(num), ["uuid", uid()]])
        node.append(["instances", ["project", Str(PROJECT), ["path", Str(self.inst_path), ["reference", Str(ref)], ["unit", 1]]]])
        self.items.append(node)
        sym = Placed(x, y, rot, pins)
        self.placed.append(sym)
        return sym

    def wire(self, *pts):
        for a, b in zip(pts, pts[1:]):
            self.items.append(["wire", ["pts", ["xy", r(a[0]), r(a[1])], ["xy", r(b[0]), r(b[1])]],
                               ["stroke", ["width", 0], ["type", "default"]], ["uuid", uid()]])

    def label(self, name, x, y, angle=0):
        just = "left" if angle in (0, 90) else "right"
        self.items.append(["label", Str(name), ["at", r(x), r(y), angle],
                           ["effects", ["font", ["size", 1.27, 1.27]], ["justify", just, "bottom"]], ["uuid", uid()]])

    def junction(self, x, y):
        self.items.append(["junction", ["at", r(x), r(y)], ["diameter", 0], ["color", 0, 0, 0, 0], ["uuid", uid()]])

    def nc(self, x, y):
        self.items.append(["no_connect", ["at", r(x), r(y)], ["uuid", uid()]])

    def text(self, s, x, y, size=1.27, bold=False):
        font = ["font", ["size", size, size]] + ([["bold", "yes"]] if bold else [])
        self.items.append(["text", Str(s), ["exclude_from_sim", "no"], ["at", x, y, 0],
                           ["effects", font, ["justify", "left", "top"]], ["uuid", uid()]])

    def box(self, x1, y1, x2, y2, dash=True):
        self.items.append(["rectangle", ["start", x1, y1], ["end", x2, y2],
                           ["stroke", ["width", 0.254], ["type", "dash" if dash else "default"]],
                           ["fill", ["type", "none"]], ["uuid", uid()]])

    def _fix_connectivity(self):
        """KiCad only connects at wire ends: split wires where a pin, label or wire end touches
        their middle, then add a junction where 3+ things meet."""
        pts = set()
        wires, others = [], []
        for it in self.items:
            if it[0] == "wire":
                a, b = it[1][1], it[1][2]
                wires.append(((float(a[1]), float(a[2])), (float(b[1]), float(b[2]))))
            else:
                others.append(it)
        for sym in self.placed:
            for num in sym._pins:
                pts.add(sym.pin(num))
        for it in others:
            if it[0] in ("label", "junction", "no_connect"):
                at = find(it, "at")
                pts.add((r(float(at[1])), r(float(at[2]))))
        for a, b in wires:
            pts.add((r(a[0]), r(a[1])))
            pts.add((r(b[0]), r(b[1])))
        new = []
        for a, b in wires:
            inner = []
            for (px, py) in pts:
                if (px, py) in ((r(a[0]), r(a[1])), (r(b[0]), r(b[1]))):
                    continue
                if abs(a[0] - b[0]) < 1e-6 and abs(px - a[0]) < 1e-6 and min(a[1], b[1]) < py < max(a[1], b[1]):
                    inner.append((px, py))
                elif abs(a[1] - b[1]) < 1e-6 and abs(py - a[1]) < 1e-6 and min(a[0], b[0]) < px < max(a[0], b[0]):
                    inner.append((px, py))
            chain = [a] + sorted(inner, key=lambda q: abs(q[0] - a[0]) + abs(q[1] - a[1])) + [b]
            new.extend(zip(chain, chain[1:]))
        cnt = {}
        for p, q in new:
            for e in (p, q):
                k = (r(e[0]), r(e[1]))
                cnt[k] = cnt.get(k, 0) + 1
        for sym in self.placed:
            for num in sym._pins:
                k = sym.pin(num)
                cnt[k] = cnt.get(k, 0) + 1
        self.items = others
        for p, q in new:
            self.wire(p, q)
        for k, c in cnt.items():
            if c >= 3:
                self.junction(*k)

    def write(self, path, title, rev, date, comments):
        self._fix_connectivity()
        doc = ["kicad_sch", ["version", 20250114], ["generator", Str("eeschema")], ["generator_version", Str("9.0")],
               ["uuid", Str(ROOT_UUID)], ["paper", Str("A2")],
               ["title_block", ["title", Str(title)], ["date", Str(date)], ["rev", Str(rev)],
                ["company", Str("Miniature World - Camelot")],
                *[["comment", i + 1, Str(c)] for i, c in enumerate(comments)]],
               ["lib_symbols", *self.libs.values()], *self.items,
               ["sheet_instances", ["path", Str("/"), ["page", Str("1")]]], ["embedded_fonts", "no"]]
        with open(path, "w", encoding="utf-8") as f:
            f.write(dump(doc) + "\n")


# ------------------------------------------------------------------ helpers
SIDES = {"L": (-1, 0, 180), "R": (1, 0, 0), "U": (0, -1, 90), "D": (0, 1, 270)}


def stub_label(s, pt, name, side, length=5.08):
    dx, dy, ang = SIDES[side]
    end = (pt[0] + dx * length, pt[1] + dy * length)
    s.wire(pt, end)
    s.label(name, *end, ang)
    return end


_pwr = [0]


def power(s, kind, pt):
    _pwr[0] += 1
    lib = {"GND": "power:GND", "+5V": "power:+5V", "FLAG": "power:PWR_FLAG"}[kind]
    ref = ("#FLG" if kind == "FLAG" else "#PWR") + f"{_pwr[0]:03d}"
    s.place(lib, ref, "PWR_FLAG" if kind == "FLAG" else kind, pt[0], pt[1], 0,
            ref_off=(0, 0), val_off=(-1.27, -3.81 if kind != "GND" else 5.08), hide_value=kind == "FLAG")


def stub_power(s, pt, kind, side, length=2.54):
    dx, dy, _ = SIDES[side]
    end = (pt[0] + dx * length, pt[1] + dy * length)
    s.wire(pt, end)
    power(s, kind, end)
    return end


def cap_v(s, ref, value, x, y_top, net_top, polar=False, label_side="U"):
    """Vertical cap: top pin to a net label, bottom pin to GND."""
    c = s.place("Device:C_Polarized" if polar else "Device:C", ref, value, x, y_top + 3.81,
                ref_off=(1.905, -1.27), val_off=(1.905, 1.27))
    if net_top:
        stub_label(s, c.pin(1), net_top, label_side, 2.54)
    stub_power(s, c.pin(2), "GND", "D", 1.27)
    return c


# ------------------------------------------------------------------ the design
# (config.h name, pin, label on control PCB, net) for the 10 limit switches
LIMITS = [
    ("K1 lance down", "D3", "B'"), ("K2 lance down", "D4", "B"),
    ("Chain CCW", "D5", "A"), ("Chain CW", "D6", "A'"),
    ("K1 lance up", "D7", "C'"), ("K2 lance up", "D8", "C"),
    ("K1 rotate CCW", "D9", "D'"), ("K2 rotate CCW", "D10", "D"),
    ("K1 rotate CW", "D11", "E'"), ("K2 rotate CW", "D12", "E"),
]


def net(pin, what):
    return f"{pin}_{what.upper().replace(' ', '_')}"


def build():
    s = Sch()
    s.text("JOUST v2 - WIRING SCHEMATIC", 20.32, 12.7, 3.0, True)
    s.text("Pins: firmware/include/config.h (source of truth). Same-name labels are connected. "
           "TBD = owner to confirm. Block diagram + pin table: docs/wiring/README.md", 20.32, 19.05, 1.6)

    # ================= POWER (x 20-205, y 30-120)
    s.box(17.78, 27.94, 208.28, 121.92)
    s.text("12 V MOTOR POWER (single supply, one fuse, one cut-off relay)", 20.32, 29.21, 1.6, True)
    ps = s.place(f"{LIBNAME}:PSU_12V", "PS1", "12 V PSU (rating TBD)", 30.48, 60.96,
                 ref_off=(-5.08, -5.08), val_off=(-5.08, 10.16))
    f1 = s.place("Device:Fuse", "F1", "3 A time-lag", 53.34, 58.42, rot=90, ref_off=(-1.27, -2.54), val_off=(-5.08, 3.81))
    s.wire(ps.pin("+V"), f1.pin("1"))
    stub_label(s, f1.pin("2"), "+12V_FUSED", "R", 7.62)
    stub_power(s, ps.pin("-V"), "GND", "D", 2.54)
    s.text("Fuse at the supply + output.\nWire gauge TBD.", 22.86, 81.28)
    # supply input caps
    cap_v(s, "C1", "330uF 35V", 78.74, 76.2, "+12V_FUSED", polar=True)
    cap_v(s, "C2", "100nF", 106.68, 76.2, "+12V_FUSED")
    s.text("C1, C2 at the supply input,\nbefore the relay. Electrolytics:\nlow-ESR, >= 25 V, mind polarity.", 76.2, 96.52)
    # motor-power relay
    k1 = s.place(f"{LIBNAME}:Relay_Module_1ch", "K1", "Motor-power relay (1-ch module)", 160.02, 55.88,
                 ref_off=(-7.62, -6.35), val_off=(-15.24, 7.62))
    stub_label(s, k1.pin("IN"), "D22_MOTOR_POWER", "L")
    stub_power(s, k1.pin("VCC"), "+5V", "U", 2.54)
    stub_power(s, k1.pin("GND"), "GND", "D", 2.54)
    stub_label(s, k1.pin("COM"), "+12V_FUSED", "R")
    stub_label(s, k1.pin("NO"), "+12V_SW", "R")
    s.nc(*k1.pin("NC"))
    s.text("COM -> NO: power only while energised.\nMust stay OFF while D22 floats (reset, upload):\n"
           "bench check. Input polarity TBD (most boards\nactive LOW). Mount location TBD.", 137.16, 73.66)
    # flags so ERC knows these nets are driven
    for i, (lbl, x) in enumerate((("+12V_FUSED", 185.42), ("+12V_SW", 195.58))):
        s.label(lbl, x, 101.6, 90)
        s.wire((x, 101.6), (x, 106.68))
        power(s, "FLAG", (x, 106.68))
    s.wire((175.26, 101.6), (175.26, 106.68))
    power(s, "FLAG", (175.26, 106.68))
    power(s, "GND", (175.26, 101.6))
    s.text("Grounds: all meet at ONE star point\n(PSU -, shield, DRV8871, Mega GND).", 137.16, 109.22)

    # ================= SHIELD + KNIGHT MOTORS (x 20-205, y 128-262)
    s.box(17.78, 127.0, 208.28, 264.16)
    s.text("MOTOR SHIELD + KNIGHT MOTORS", 20.32, 128.27, 1.6, True)
    u2 = s.place(f"{LIBNAME}:MotorShield_V2", "U2", "Adafruit Motor Shield V2", 55.88, 172.72,
                 ref_off=(-3.81, -17.78), val_off=(-15.24, 17.78))
    stub_label(s, u2.pin("SDA"), "I2C_SDA", "L")
    stub_label(s, u2.pin("SCL"), "I2C_SCL", "L")
    stub_power(s, u2.pin("5V"), "+5V", "U", 2.54)
    stub_power(s, u2.pin("GND"), "GND", "D", 2.54)
    stub_label(s, u2.pin("VMOT+"), "+12V_SW", "L")
    stub_power(s, u2.pin("VMOT-"), "GND", "D", 2.54)
    s.text("Stacked on the Mega: SDA/SCL/5V/GND\nvia the R3 headers. Vin jumper OFF\n(12 V never reaches Mega VIN).\n"
           "A4/A5 tie to SDA/SCL: unverified.", 22.86, 200.66)
    cap_v(s, "C3", "100uF 35V", 30.48, 222.25, "+12V_SW", polar=True)
    cap_v(s, "C4", "100nF", 50.8, 222.25, "+12V_SW")
    s.text("C3, C4 at the shield motor terminal.", 22.86, 243.84)
    motors = [("M1", "K1 lance", "MOT1", "C5"), ("M2", "K1 rotate", "MOT2", "C6"),
              ("M3", "K2 lance", "MOT3", "C7"), ("M4", "K2 rotate", "MOT4", "C8")]
    for i, (port, name, ref, cref) in enumerate(motors):
        y = 144.78 + 27.94 * (i % 2)
        x = 120.65 if i < 2 else 167.64
        a, b = u2.pin(f"{port}A"), u2.pin(f"{port}B")
        stub_label(s, a, f"{port}A", "R", 2.54)
        stub_label(s, b, f"{port}B", "R", 2.54)
        m = s.place("Motor:Motor_DC", ref, f"{name} 12 V", x, y, ref_off=(3.81, -1.27), val_off=(-7.62, 12.7))
        c = s.place("Device:C", cref, "100nF", x + 12.7, y - 1.27, ref_off=(1.905, -1.27), val_off=(1.905, 1.27))
        s.wire(m.pin("1"), (x + 12.7, y - 5.08))
        s.wire(m.pin("2"), (x + 12.7, y + 7.62), c.pin("2"))
        stub_label(s, m.pin("1"), f"{port}A", "L", 5.08)
        s.wire(m.pin("2"), (x - 5.08, y + 7.62))
        s.label(f"{port}B", x - 5.08, y + 7.62, 180)
    s.text("100nF ceramic directly across each motor's terminals.\nNEVER an electrolytic across a motor.\n"
           "Leads twisted. Lance: down = FORWARD. Rotate: CW = BACKWARD.", 104.14, 236.22)

    # ================= CHAIN DRIVE (x 20-205, y 270-372)
    s.box(17.78, 269.24, 208.28, 373.38)
    s.text("CHAIN DRIVE: relay board today -> DRV8871 at Task 9 (only one fitted)", 20.32, 270.51, 1.6, True)
    k2 = s.place(f"{LIBNAME}:Relay_Module_2ch", "K2", "2-ch relay board (today)", 50.8, 297.18,
                 ref_off=(-5.08, -11.43), val_off=(-12.7, 11.43))
    stub_label(s, k2.pin("IN1"), "A0_CHAIN_RELAY_CCW", "L")
    stub_label(s, k2.pin("IN2"), "A1_CHAIN_RELAY_CW", "L")
    stub_power(s, k2.pin("VCC"), "+5V", "U", 2.54)
    stub_power(s, k2.pin("GND"), "GND", "D", 2.54)
    stub_label(s, k2.pin("V+"), "+12V_SW", "R")
    stub_power(s, k2.pin("V-"), "GND", "D", 2.54)
    stub_label(s, k2.pin("MOT_A"), "CHAIN_A", "R")
    stub_label(s, k2.pin("MOT_B"), "CHAIN_B", "R")
    s.text("Active LOW. Contact wiring inside\nthe board TBD (owner). Brake or\ncoast at stop: TBD.", 22.86, 317.5)
    u3 = s.place(f"{LIBNAME}:DRV8871_Module", "U3", "Adafruit DRV8871 (3190)", 50.8, 345.44,
                 ref_off=(-3.81, -8.89), val_off=(-11.43, 8.89))
    stub_label(s, u3.pin("IN1"), "D44_CHAIN_IN1", "L")
    stub_label(s, u3.pin("IN2"), "D45_CHAIN_IN2", "L")
    stub_power(s, u3.pin("GND"), "GND", "D", 2.54)
    stub_label(s, u3.pin("VM"), "+12V_SW", "R")
    stub_power(s, u3.pin("PGND"), "GND", "D", 2.54)
    stub_label(s, u3.pin("OUT1"), "CHAIN_A", "R")
    stub_label(s, u3.pin("OUT2"), "CHAIN_B", "R")
    s.text("Board pin names unverified.\nWhich IN drives CW: bench.", 22.86, 363.22)
    cap_v(s, "C9", "100uF 35V", 109.22, 330.2, "+12V_SW", polar=True)
    cap_v(s, "C10", "100nF", 129.54, 330.2, "+12V_SW")
    s.text("C9, C10 at DRV8871 VM.", 104.14, 350.52)
    m5 = s.place("Motor:Motor_DC", "MOT5", "Chain 12 V", 157.48, 297.18, ref_off=(3.81, -1.27), val_off=(-7.62, 12.7))
    c11 = s.place("Device:C", "C11", "100nF", 170.18, 295.91, ref_off=(1.905, -1.27), val_off=(1.905, 1.27))
    s.wire(m5.pin("1"), (170.18, 292.1))
    s.wire(m5.pin("2"), (170.18, 304.8), c11.pin("2"))
    stub_label(s, m5.pin("1"), "CHAIN_A", "L", 5.08)
    s.wire(m5.pin("2"), (152.4, 304.8))
    s.label("CHAIN_B", 152.4, 304.8, 180)

    # ================= MEGA (x 215-360, y 28-262)
    s.box(215.9, 27.94, 360.68, 264.16)
    s.text("CONTROLLER (Mega on USB; shield stacked on top)", 218.44, 29.21, 1.6, True)
    u1 = s.place(f"{LIBNAME}:Arduino_Mega2560", "U1", "Arduino Mega 2560 R3", 287.02, 144.78,
                 ref_off=(-12.7, -30.48), val_off=(-11.43, 34.29))
    for name, pin, _ in LIMITS:
        stub_label(s, u1.pin(pin), net(pin, name), "L", 10.16)
    for pin, name in (("D2", "start"), ("A2", "panel rot cw"), ("A3", "panel rot ccw"),
                      ("A10", "panel chain cw"), ("A11", "panel chain ccw"),
                      ("D23", "panel lances up"), ("D24", "panel lances down"),
                      ("A9", "key"), ("A12", "safe op")):
        stub_label(s, u1.pin(pin), net(pin, name), "L", 10.16)
    for pin, nm in (("D13", "D13_START_LED"), ("D22", "D22_MOTOR_POWER"), ("A0", "A0_CHAIN_RELAY_CCW"),
                    ("A1", "A1_CHAIN_RELAY_CW"), ("D44", "D44_CHAIN_IN1"), ("D45", "D45_CHAIN_IN2"),
                    ("D18", "D18_TX1"), ("D19", "D19_RX1"), ("D20", "I2C_SDA"), ("D21", "I2C_SCL")):
        stub_label(s, u1.pin(pin), nm, "R", 7.62)
    stub_power(s, u1.pin("5V"), "+5V", "U", 2.54)
    stub_power(s, u1.pin("GND"), "GND", "D", 2.54)
    s.nc(*u1.pin("USB"))
    p5 = u1.pin("5V")
    s.wire((p5[0], p5[1] - 2.54), (p5[0] + 7.62, p5[1] - 2.54), (p5[0] + 7.62, p5[1]))
    power(s, "FLAG", (p5[0] + 7.62, p5[1]))
    s.text("USB = power + serial (115200).\nD0/D1 USB serial; D18/D19 ESP32;\nD20/D21 I2C; D44-D46 Timer5;\n"
           "D50-D53 SPI: reserved, don't assign.", 218.44, 231.14)
    s.text("All inputs: switch to GND,\nactive LOW, pull-up.", 218.44, 50.8)

    # ================= LIMIT INPUT FILTERS (x 366-585, y 28-200)
    s.box(365.76, 27.94, 584.2, 203.2)
    s.text("LIMIT INPUT FILTERS (protoboard near the Mega), x10: 1k pull-up to 5 V, 1k series, 100nF at the pin",
           368.3, 29.21, 1.6, True)
    for i, (name, pin, plabel) in enumerate(LIMITS):
        col, row = i % 5, i // 5
        ox, oy = 375.92 + 41.91 * col, 50.8 + 80.01 * row
        n = 12 + i * 2
        rpu = s.place("Device:R", f"R{n - 1}", "1k", ox, oy, ref_off=(1.905, -1.27), val_off=(1.905, 1.27))
        stub_power(s, rpu.pin("1"), "+5V", "U", 2.54)
        node = rpu.pin("2")
        rs = s.place("Device:R", f"R{n}", "1k", ox + 7.62, node[1], rot=90, ref_off=(-1.905, 2.54), val_off=(-1.905, 4.445))
        s.wire(node, rs.pin("1"))
        c = s.place("Device:C", f"C{12 + i}", "100nF", ox + 17.78, node[1] + 3.81, ref_off=(1.905, -1.27), val_off=(1.905, 1.27))
        s.wire(rs.pin("2"), c.pin("1"), (ox + 27.94, node[1]))
        s.label(net(pin, name), ox + 27.94, node[1], 90)
        stub_power(s, c.pin("2"), "GND", "D", 1.27)
        sw = s.place("Switch:SW_Push", f"SW{i + 1}", f"{name} [{plabel}]", ox, node[1] + 15.24, rot=90,
                     ref_off=(2.54, 0), val_off=(2.54, 7.62))
        s.wire(node, sw.pin("2"))
        stub_power(s, sw.pin("1"), "GND", "D", 1.27)
    s.text("SW1-10: bolt on the axle touches a post on a small PCB; the bolt is grounded through a brush.\n"
           "~5 mA wetting current per closed switch. Filter parts R11..R30 and C12..C21 sit at the Mega end.",
           368.3, 185.42, 1.27)

    # ================= PANEL, START, KEY, LED (x 215-360, y 270-372)
    s.box(215.9, 269.24, 360.68, 373.38)
    s.text("OPERATOR INPUTS + INDICATOR (internal pull-ups, no filter)", 218.44, 270.51, 1.6, True)
    btns = [("D2", "start", "Start button"), ("A9", "key", "Key switch"), ("A2", "panel rot cw", "Both rotate CW"),
            ("A3", "panel rot ccw", "Both rotate CCW"), ("A10", "panel chain cw", "Chain CW"),
            ("A11", "panel chain ccw", "Chain CCW"), ("D23", "panel lances up", "Lances up (TBD pin)"),
            ("D24", "panel lances down", "Lances down (TBD pin)")]
    for i, (pin, nm, desc) in enumerate(btns):
        x, y = 241.3 + 0 * i, 281.94 + 8.89 * i
        sw = s.place("Switch:SW_Push", f"SW{11 + i}", desc, x, y, ref_off=(-2.54, -3.81), val_off=(10.16, 0))
        s.wire(sw.pin("1"), (x - 7.62, y))
        s.label(net(pin, nm), x - 7.62, y, 180)
        s.wire(sw.pin("2"), (x + 7.62, y))
        power(s, "GND", (x + 7.62, y))
    jp = s.place("Connector_Generic:Conn_01x02", "JP1", "Safe-op jumper", 330.2, 287.02, ref_off=(-2.54, -5.08), val_off=(-7.62, 6.35))
    stub_label(s, jp.pin("1"), net("A12", "safe op"), "L", 7.62)
    stub_power(s, jp.pin("2"), "GND", "L", 7.62)
    rl = s.place("Device:R", "R1", "220R (TBD)", 320.04, 320.04, rot=90, ref_off=(-1.27, -3.81), val_off=(-3.81, 3.81))
    d1 = s.place("Device:LED", "D1", "Start-button LED", 335.28, 320.04, rot=180, ref_off=(-1.27, -3.81), val_off=(-3.81, 3.81))
    stub_label(s, rl.pin("1"), "D13_START_LED", "L", 5.08)
    s.wire(rl.pin("2"), d1.pin("2"))
    s.wire(d1.pin("1"), (345.44, 320.04), (345.44, 322.58))
    power(s, "GND", (345.44, 322.58))
    s.text("Start LED: D13 (also onboard LED).\nHow it is wired: TBD (owner).\nLance fault 500/500 ms,\n"
           "other fault 2000/2000 ms.", 304.8, 332.74)
    s.text("Lances up/down moved off A4/A5\n(removed) to D23/D24: tentative.", 304.8, 355.6)

    # ================= FUTURE ESP32 (x 366-585, y 210-300)
    s.box(365.76, 208.28, 584.2, 299.72)
    s.text("FUTURE (stretch S1): ESP32 on Serial1 via level shifter - not fitted", 368.3, 209.55, 1.6, True)
    ls = s.place(f"{LIBNAME}:LevelShifter", "U4", "Level shifter (future)", 431.8, 246.38, dnp=True,
                 ref_off=(-3.81, -8.89), val_off=(-11.43, 8.89))
    stub_label(s, ls.pin("HV1"), "D18_TX1", "L")
    stub_label(s, ls.pin("HV2"), "D19_RX1", "L")
    stub_power(s, ls.pin("HV"), "+5V", "U", 2.54)
    stub_power(s, ls.pin("GND"), "GND", "D", 2.54)
    stub_label(s, ls.pin("LV1"), "ESP_RX", "R")
    stub_label(s, ls.pin("LV2"), "ESP_TX", "R")
    s.nc(*ls.pin("LV"))
    esp = s.place(f"{LIBNAME}:ESP32_Link", "U5", "ESP32 (future)", 520.7, 246.38, dnp=True,
                  ref_off=(-3.81, -6.35), val_off=(-7.62, 6.35))
    stub_label(s, esp.pin("RX"), "ESP_RX", "L")
    stub_label(s, esp.pin("TX"), "ESP_TX", "L")
    stub_power(s, esp.pin("GND"), "GND", "D", 2.54)
    s.text("Mega TX1 (D18) -> ESP32 RX; ESP32 TX -> Mega RX1 (D19).\nESP32 is 3.3 V: 5 V into it is over rating. "
           "3.3 V into the Mega is marginal\n(check ATmega2560 VIH). LV rail = ESP32 3V3 (not drawn).", 368.3, 274.32)

    # ================= OPEN ITEMS
    s.text("OPEN ITEMS (owner):\n1. 12 V supply current rating; wire gauges.\n2. Physical location of panel, key, start "
           "button; connectors per harness.\n3. Does the shield tie A4/A5 to SDA/SCL? (continuity check, power off)\n"
           "4. Relay board mounting and input polarity.\n5. Start LED wiring to D13.\n6. Chain relay board contact wiring.",
           368.3, 307.34, 1.5)
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--date", default="2026-11-08")
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    sch = os.path.join(OUT, f"{PROJECT}.kicad_sch")
    if os.path.exists(sch) and not args.force:
        sys.exit(f"{sch} exists; use --force to overwrite (this discards manual edits)")
    s = build()
    s.write(sch, "Joust v2 - wiring", "0.1", args.date,
            ["Generated by docs/wiring/tools/gen_schematic.py - edit in KiCad from here on",
             "Pins: firmware/include/config.h. Draft for owner review."])
    lib = ["kicad_symbol_lib", ["version", 20241209], ["generator", Str("gen_schematic.py")],
           ["generator_version", Str("9.0")], *(b() for b in SYMBOLS.values())]
    with open(os.path.join(OUT, f"{PROJECT}.kicad_sym"), "w") as f:
        f.write(dump(lib) + "\n")
    with open(os.path.join(OUT, "sym-lib-table"), "w") as f:
        f.write('(sym_lib_table\n\t(version 7)\n\t(lib (name "%s")(type "KiCad")(uri "${KIPRJMOD}/%s.kicad_sym")'
                '(options "")(descr "Joust project symbols"))\n)\n' % (LIBNAME, PROJECT))
    pro = os.path.join(OUT, f"{PROJECT}.kicad_pro")
    if not os.path.exists(pro):
        with open(pro, "w") as f:
            f.write('{\n  "meta": {\n    "filename": "%s.kicad_pro",\n    "version": 1\n  }\n}\n' % PROJECT)
    print("wrote", sch)


if __name__ == "__main__":
    main()
