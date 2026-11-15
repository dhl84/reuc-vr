# Finds code addresses stored in data sections that the translated game does not know as an entry.
# Prints `entry <hex>` lines for the project configuration.
#
# Usage: python -I tools/find_data_entries.py --dol <main.dol> --table <generated dir>/generation_*/game_table.cpp --jump-targets <file>
#
# <file> holds the output of `xref --dol <main.dol> --cfg <cfg> --jump-targets`: the targets of the switch tables that the analyzer
# found. A data word is a candidate when it is 4-aligned, points into a text section at a word that is not padding and that follows a
# terminator (blr, b, bctr) or padding, is not a known entry, and is not a switch target. The analyzer adds such pointers by itself
# after a blr; it leaves out a pointer after a plain `b` when the target has no prologue and its neighbours are not code pointers.
import argparse
import re
import struct
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dol", required=True)
    ap.add_argument("--table", required=True)
    ap.add_argument("--jump-targets", required=True)
    args = ap.parse_args()
    d = Path(args.dol).read_bytes()
    known = {int(h, 16) for h in re.findall(r"\{0x([0-9A-Fa-f]{8})u,", Path(args.table).read_text(encoding="utf-8"))}
    jump = {int(line, 16) for line in Path(args.jump_targets).read_text(encoding="utf-8", errors="replace").split() if line.strip()}
    offs = struct.unpack(">18I", d[0:0x48])
    addrs = struct.unpack(">18I", d[0x48:0x90])
    sizes = struct.unpack(">18I", d[0x90:0xD8])
    word = {}
    for i in range(7):
        if sizes[i]:
            n = sizes[i] // 4
            for k, x in enumerate(struct.unpack(">%dI" % n, d[offs[i]:offs[i] + 4 * n])):
                word[addrs[i] + 4 * k] = x
    out = {}
    for i in range(7, 18):
        if not sizes[i]:
            continue
        n = sizes[i] // 4
        for k, v in enumerate(struct.unpack(">%dI" % n, d[offs[i]:offs[i] + 4 * n])):
            if (v & 3) or v not in word or v in known or v in jump or word[v] == 0:
                continue
            p = word.get(v - 4, 0)
            term = "blr" if p == 0x4E800020 else "b" if (p >> 26) == 18 and not (p & 1) else "bctr" if p == 0x4E800420 else "padding" if p == 0 else None
            if term:
                out.setdefault(v, (addrs[i] + 4 * k, term))
    for v in sorted(out):
        print("entry %08X   # pointer in data at %08X, after %s" % (v, out[v][0], out[v][1]))


if __name__ == "__main__":
    main()
