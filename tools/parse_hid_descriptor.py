#!/usr/bin/env python3
"""Minimal HID report-descriptor decoder (short items only), for evidence review."""
import sys

TYPES = {0: "Main", 1: "Global", 2: "Local", 3: "Reserved"}
MAIN = {0x8: "Input", 0x9: "Output", 0xB: "Feature", 0xA: "Collection", 0xC: "End Collection"}
GLOBAL = {0x0: "Usage Page", 0x1: "Logical Minimum", 0x2: "Logical Maximum",
          0x3: "Physical Minimum", 0x4: "Physical Maximum", 0x5: "Unit Exponent",
          0x6: "Unit", 0x7: "Report Size", 0x8: "Report ID", 0x9: "Report Count",
          0xA: "Push", 0xB: "Pop"}
LOCAL = {0x0: "Usage", 0x1: "Usage Minimum", 0x2: "Usage Maximum",
         0x3: "Designator Index", 0x4: "Designator Minimum", 0x5: "Designator Maximum",
         0x7: "String Index", 0x8: "String Minimum", 0x9: "String Maximum", 0xA: "Delimiter"}
COLL = {0x00: "Physical", 0x01: "Application", 0x02: "Logical", 0x03: "Report",
        0x04: "Named Array", 0x05: "Usage Switch", 0x06: "Usage Modifier"}

def decode(hexstr):
    data = bytes.fromhex(hexstr)
    i = 0
    usage_page = 0
    rid = None
    rsize = None
    rcount = None
    lmin = lmax = None
    out = []
    while i < len(data):
        prefix = data[i]; i += 1
        size = prefix & 3
        size = 4 if size == 3 else size
        typ = (prefix >> 2) & 3
        tag = (prefix >> 4) & 0xF
        if size and i + size > len(data):
            out.append("  TRUNCATED"); break
        val = int.from_bytes(data[i:i+size], "little") if size else None
        i += size
        if typ == 0:
            name = MAIN.get(tag, f"Main?0x{tag:X}")
            extra = ""
            if name == "Collection":
                extra = f" ({COLL.get(val, hex(val) if val is not None else '?')})"
            elif name in ("Input", "Output", "Feature") and val is not None:
                flags = []
                for bit, label in [(0, "Constant"), (1, "Variable"), (2, "Relative"), (3, "Wrap"),
                                   (4, "NonLinear"), (5, "NoPreferred"), (6, "NullState"), (7, "Volatile")]:
                    if val & (1 << bit): flags.append(label)
                extra = " [" + ",".join(flags) + "]"
            out.append(f"  {name}{extra}")
        elif typ == 1:
            name = GLOBAL.get(tag, f"Global?0x{tag:X}")
            if name == "Usage Page": usage_page = val
            elif name == "Report ID": rid = val
            elif name == "Report Size": rsize = val
            elif name == "Report Count": rcount = val
            elif name == "Logical Minimum": lmin = val
            elif name == "Logical Maximum": lmax = val
            if name == "Usage Page":
                out.append(f"  {name} = 0x{val:04X}")
            else:
                out.append(f"  {name} = {val}" + (f" (0x{val:X})" if val is not None else ""))
        elif typ == 2:
            name = LOCAL.get(tag, f"Local?0x{tag:X}")
            if val is not None and size == 3:
                up, uid = val >> 16, val & 0xFFFF
                out.append(f"  {name} = (page 0x{up:04X}) usage 0x{uid:04X}")
            else:
                out.append(f"  {name} = 0x{val:04X} (page 0x{usage_page:04X})" if val is not None else f"  {name}")
        else:
            out.append(f"  Reserved")
        if typ == 0 and tag in (0x8, 0x9, 0xB):
            out.append(f"    -> report_id={rid} size={rsize} count={rcount} bits={None if (rsize is None or rcount is None) else rsize*rcount} logical={lmin}..{lmax}")
    return "\n".join(out)

if __name__ == "__main__":
    print(decode(sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read().strip()))
