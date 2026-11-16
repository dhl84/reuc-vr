# Finds function addresses that code builds with `lis` + `addi` (or `ori`) too far apart for the analyzer (which looks 32 bytes back),
# and that the translated game does not know as an entry. Prints `entry <hex>` lines for the project configuration.
#
# Usage: python -I tools/find_entries.py --dol <main.dol> --table <generated dir>/generation_*/game_table.cpp [--window 1024]
#
# A candidate must lie in a text section, be 4-aligned, follow a terminator (blr, b, bctr, rfi) or padding, and not start with padding.
# The register that holds the `lis` value is followed through the function: the pair must be in one function (no blr between them).
# The value is also followed through a word of the stack frame (`stw rS,d(r1)`, later `lwz rD,d(r1)`): a large constructor keeps the
# high half there while it builds many addresses.
import argparse
import re
import struct
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dol", required=True)
    ap.add_argument("--table", required=True)
    ap.add_argument("--window", type=int, default=1024)
    args = ap.parse_args()
    d = Path(args.dol).read_bytes()
    offs = struct.unpack(">18I", d[0:0x48])
    addrs = struct.unpack(">18I", d[0x48:0x90])
    sizes = struct.unpack(">18I", d[0x90:0xD8])
    text = []
    word = {}
    for i in range(7):
        if sizes[i]:
            n = sizes[i] // 4
            w = struct.unpack(">%dI" % n, d[offs[i]:offs[i] + 4 * n])
            text.append((addrs[i], w))
            for k, x in enumerate(w):
                word[addrs[i] + 4 * k] = x
    known = {int(h, 16) for h in re.findall(r"\{0x([0-9A-Fa-f]{8})u,", Path(args.table).read_text(encoding="utf-8"))}

    def terminator(x):
        return x in (0x4E800020, 0x4E800420, 0x4C000064, 0) or ((x >> 26) == 18 and not (x & 1))

    found = {}
    for base, w in text:
        hi = {}  # register -> (value, pc)
        slot = {}  # displacement from r1 -> value of a `lis` that the function stored there
        for k, x in enumerate(w):
            pc = base + 4 * k
            op, rd, ra, imm = x >> 26, (x >> 21) & 31, (x >> 16) & 31, x & 0xFFFF
            if x == 0x4E800020:
                hi.clear()
                slot.clear()
                continue
            v = None
            if op == 36 and ra == 1:  # stw rS,d(r1)
                if rd in hi:
                    slot[imm] = hi[rd][0]
                else:
                    slot.pop(imm, None)
                continue
            if op == 32 and ra == 1 and imm in slot:  # lwz rD,d(r1)
                hi[rd] = (slot[imm], pc)
                continue
            if op == 15 and ra == 0:
                hi[rd] = (imm, pc)
                continue
            if op == 14 and ra in hi and pc - hi[ra][1] <= args.window:
                v = ((hi[ra][0] << 16) + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF
            elif op == 24 and rd in hi and pc - hi[rd][1] <= args.window:  # ori rA,rS,imm
                v = (hi[rd][0] << 16) | imm
            # writes end the tracking of the written register
            if op in (14, 15, 12, 13, 7, 8) or 32 <= op <= 35 or 40 <= op <= 43:
                if not (op == 14 and v is not None and rd == ra):
                    hi.pop(rd, None)
                else:
                    hi.pop(rd, None)
            elif op in (24, 25, 26, 27, 28, 29):
                hi.pop(ra, None)
            elif op == 31:
                hi.pop(rd, None)
                hi.pop(ra, None)
            elif op == 18 and (x & 1):
                for r in (0, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12):  # a call clobbers the volatile registers
                    hi.pop(r, None)
            if v is None or (v & 3) or v not in word or v in known:
                continue
            if word[v] == 0 or not terminator(word.get(v - 4, 0)):
                continue
            found.setdefault(v, pc)
    for v in sorted(found):
        print("entry %08X   # address built at %08X" % (v, found[v]))


if __name__ == "__main__":
    main()
