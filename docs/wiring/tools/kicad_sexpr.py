"""Minimal S-expression reader/writer for KiCad files (.kicad_sym, .kicad_sch, .kicad_pcb).

KiCad files are nested lists: (token arg arg (child ...) ...). Strings are double-quoted.
We keep quoted strings as `Str` so they are written back quoted, and bare atoms as plain str.
"""
from __future__ import annotations

import re

_TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))', re.S)


class Str(str):
    """A quoted string atom."""


def parse(text: str):
    stack, cur = [], None
    pos = 0
    n = len(text)
    while pos < n:
        m = _TOKEN.match(text, pos)
        if not m:
            if text[pos:].strip() == "":
                break
            raise ValueError(f"parse error at {pos}: {text[pos:pos + 40]!r}")
        pos = m.end()
        lp, rp, qs, atom = m.groups()
        if lp:
            new = []
            if cur is not None:
                cur.append(new)
            stack.append(new)
            cur = new
        elif rp:
            done = stack.pop()
            if not stack:
                return done
            cur = stack[-1]
        elif qs is not None:
            cur.append(Str(_unescape(qs)))
        else:
            cur.append(atom)
    raise ValueError("unbalanced s-expression")


def _unescape(s: str) -> str:
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            nxt = s[i + 1]
            out.append({"n": "\n", "t": "\t"}.get(nxt, nxt))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _q(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def dump(node, indent: int = 0) -> str:
    """Serialise with KiCad-like formatting (one child list per line)."""
    if not isinstance(node, list):
        return _q(node) if isinstance(node, Str) else str(node)
    pad = "\t" * indent
    atoms, kids = [], []
    for item in node:
        (kids if isinstance(item, list) else atoms).append(item)
    head = "(" + " ".join(dump(a) for a in atoms)
    if not kids:
        return pad + head + ")"
    # short lists like (xy 1 2) or (at 1 2 0) stay on one line
    if all(not isinstance(k, list) or not any(isinstance(x, list) for x in k) for k in kids) and len(kids) <= 2 \
            and sum(len(dump(k)) for k in kids) < 80 and atoms and atoms[0] in ("pts", "stroke", "font", "effects"):
        return pad + head + " " + " ".join(dump(k) for k in kids) + ")"
    out = [pad + head]
    for k in kids:
        out.append(dump(k, indent + 1))
    out.append(pad + ")")
    return "\n".join(out)


def find(node, token):
    """First direct child list whose head is `token`."""
    for item in node:
        if isinstance(item, list) and item and item[0] == token:
            return item
    return None


def find_all(node, token):
    return [i for i in node if isinstance(i, list) and i and i[0] == token]


def load_lib_symbol(lib_path: str, name: str):
    """Return the (symbol "name" ...) node from a .kicad_sym library, flattening `extends`."""
    lib = parse(open(lib_path, encoding="utf-8").read())
    syms = {s[1]: s for s in find_all(lib, "symbol")}
    sym = syms[name]
    ext = find(sym, "extends")
    if ext is None:
        return sym
    base = syms[ext[1]]
    # derived symbol: base graphics/pins + derived properties
    flat = [sym[0], Str(name)]
    props = {p[1]: p for p in find_all(sym, "property")}
    for item in base[2:]:
        if isinstance(item, list) and item[0] == "property" and item[1] in props:
            flat.append(props.pop(item[1]))
        elif isinstance(item, list) and item[0] == "symbol":
            sub = list(item)
            sub[1] = Str(str(item[1]).replace(str(base[1]), name, 1))
            flat.append(sub)
        else:
            flat.append(item)
    flat.extend(props.values())
    return flat


def symbol_pins(sym):
    """{pin_number: (x, y, angle, name)} in symbol (y-up) coordinates for all units."""
    pins = {}
    for unit in find_all(sym, "symbol"):
        for p in find_all(unit, "pin"):
            at = find(p, "at")
            num = find(p, "number")[1]
            nm = find(p, "name")[1]
            pins[str(num)] = (float(at[1]), float(at[2]), float(at[3]) if len(at) > 3 else 0.0, str(nm))
    return pins
