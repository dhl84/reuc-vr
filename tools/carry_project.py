# Writes the project files of a related game from the reference project and the outputs of tools/dolmatch.py and tools/sigmatch.
#
# Usage:
#   python -I tools/carry_project.py --project <ref project dir> --match <dolmatch out dir> --sigmatch <sigmatch names.txt> --out <new project dir>
#
# Written to <new project dir>:
#   identified_sdk.txt   the sigmatch names of the new program (unverified, as in the reference project)
#   symbols.txt          names carried from the reference project, then the lines of symbols_manual.txt (kept by hand, optional)
#   hle_map.txt          every line of the reference hle_map.txt, as "@<new address> <HleName>", or a comment that says why it is off
#   hle_status.txt       one line per reference entry: on / off, the reason, and how the address was found
# Kept by hand in <new project dir> and read here:
#   hle_exclude.txt      regular expressions of HLE names that stay off (natives that hold addresses or logic of the reference game)
#   hle_manual.txt       "@<address> <HleName>" lines found by hand; they win over carried addresses
#   symbols_manual.txt   "<address> <name> manual <evidence>" lines
import argparse
import collections
import re
import sys
from pathlib import Path


def read_pairs(path):
    out = []
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            if line and not line.startswith("#"):
                p = line.split()
                if len(p) >= 2:
                    out.append((int(p[0], 16), p[1], p[2:]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", required=True)
    ap.add_argument("--match", required=True)
    ap.add_argument("--sigmatch", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    ref, match, out = Path(args.project), Path(args.match), Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    # reference: name -> addresses, as tools/hle_cfg.py reads them
    ref_names = collections.defaultdict(set)
    for fn in ("identified_sdk.txt", "symbols.txt", "gx_symbols.txt"):
        for a, n, _ in read_pairs(ref / fn):
            ref_names[n].add(a)

    carried = {}  # ref addr -> (new addr, method)
    for line in (match / "matched_functions.txt").read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            p = line.split()
            carried[int(p[1], 16)] = (int(p[0], 16), p[2])

    sig = read_pairs(Path(args.sigmatch))
    sig_names = collections.defaultdict(set)
    for a, n, _ in sig:
        sig_names[n].add(a)

    manual_hle = {}
    p = out / "hle_manual.txt"
    if p.exists():
        for raw in p.read_text(encoding="utf-8").splitlines():
            line = raw.split("#", 1)[0].strip()
            if line.startswith("@"):
                q = line.split()
                manual_hle[q[1]] = int(q[0][1:], 16)
    exclude = []
    p = out / "hle_exclude.txt"
    if p.exists():
        exclude = [re.compile(l.strip()) for l in p.read_text(encoding="utf-8").splitlines() if l.strip() and not l.startswith("#")]
    manual_syms = read_pairs(out / "symbols_manual.txt")
    manual_sym_names = collections.defaultdict(set)
    for a, n, _ in manual_syms:
        manual_sym_names[n].add(a)

    # identified_sdk.txt and symbols.txt
    (out / "identified_sdk.txt").write_text(
        "# Functions identified in main.dol by signature (Dolphin totaldb.dsy, min 3 words). Format: <addr> <name>\n"
        "# Names from this database can be wrong; verify before relying on one.\n"
        + "\n".join("%08X %s" % (a, n) for a, n, _ in sig) + "\n", encoding="utf-8")
    lines = ["# Names carried from the reference project by tools/dolmatch.py: <addr> <name> carried <method> from <ref addr>"]
    for a, n, rest in sorted(read_pairs(match / "carried_symbols.txt")):
        lines.append("%08X %s %s" % (a, n, " ".join(rest)))
    lines.append("# Manual symbols (symbols_manual.txt)")
    for a, n, rest in manual_syms:
        lines.append("%08X %s %s" % (a, n, " ".join(rest)))
    (out / "symbols.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    hle, status = [], []
    counts = collections.Counter()
    seen_manual = set()
    for raw in (ref / "hle_map.txt").read_text(encoding="utf-8").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            hle.append(raw)
            continue
        q = line.split()
        if q[0].startswith("@"):
            sdk, name, ref_addrs = None, q[1], {int(q[0][1:], 16)}
        else:
            sdk, name = q[0], (q[1] if len(q) > 1 else q[0])
            ref_addrs = ref_names.get(sdk, set())
        label = sdk or name
        key = sdk or name  # manual lines name the SDK function, or the HLE name for "@" lines
        new, how = None, None
        if key in manual_hle:
            new, how = manual_hle[key], "manual"
            seen_manual.add(key)
        elif sdk and len(manual_sym_names.get(sdk, ())) == 1:
            new, how = next(iter(manual_sym_names[sdk])), "manual symbol"
        else:
            c = {carried[a] for a in ref_addrs if a in carried}
            s = sig_names.get(sdk, set()) if sdk else set()
            if len(c) == 1:
                new, how = next(iter(c))
                how = "carried %s" % how
                if len(s) == 1 and next(iter(s)) != new:
                    new, how = None, "conflict: carried %08X, sigmatch %08X" % (next(iter(c))[0], next(iter(s)))
                elif len(s) == 1:
                    how += " + sigmatch"
            elif len(s) == 1:
                new, how = next(iter(s)), "sigmatch"
            else:
                how = "not found"
        off = any(rx.search(name) for rx in exclude)
        if new is None:
            hle.append("# OFF (%s): %s" % (how, line))
            status.append("off  %-28s %s" % (label, how))
            counts["off: not found"] += 1
        elif off:
            hle.append("# OFF (excluded, reference game logic): @%08X %s  # %s, %s" % (new, name, label, how))
            status.append("off  %-28s excluded; %08X %s" % (label, new, how))
            counts["off: excluded"] += 1
        else:
            hle.append("@%08X %s  # %s, %s" % (new, name, label, how))
            status.append("on   %-28s %08X %s" % (label, new, how))
            counts["on"] += 1
    extra = [k for k in manual_hle if k not in seen_manual]
    if extra:
        hle.append("")
        hle.append("# --- Manual entries that the reference map does not have (hle_manual.txt)")
        for k in extra:
            hle.append("@%08X %s  # manual" % (manual_hle[k], k))
            counts["on"] += 1
    (out / "hle_map.txt").write_text("\n".join(hle) + "\n", encoding="utf-8")
    (out / "hle_status.txt").write_text("\n".join(status) + "\n", encoding="utf-8")
    print(dict(counts))


if __name__ == "__main__":
    main()
