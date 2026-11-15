# Carries known function addresses from one Wii main.dol to a related one (a sequel, another build).
#
# Usage:
#   python -I tools/dolmatch.py --ref <ref main.dol> --new <new main.dol> --project <ref project dir> --out <out dir>
#
# The reference project directory holds identified_sdk.txt, symbols.txt, gx_symbols.txt and hle_map.txt.
# Outputs in <out dir>:
#   matched_functions.txt   <new addr> <ref addr> <method> <size words> [name]
#   carried_symbols.txt     <new addr> <name> carried <method> from <ref addr>     (same format as symbols.txt)
#   carried_hle_addresses.txt   every "@address Name" line of hle_map.txt with the address in the new DOL, or "missing"
#   sda_offsets.txt         r13 / r2 displacement pairs seen at the same place in matched functions, with vote counts
#   same_code.txt           functions that are the same code in both programs, as ranges
#   data_addresses.txt      absolute data address pairs (lis + low half) seen at the same place in matched functions
#
# Method, in order:
#   exact   the function body is the same after relocations are masked, and that body is unique in both programs
#   call    the two functions are called from the same place of two functions that were already matched
#   order   the functions lie between two matched neighbours, with the same count and the same sizes on both sides
#   inside  the address lies inside a function that is the same code in both programs
#   pattern the masked instruction run from the address to its first blr stands exactly once in the new program
# No game code or data is written to the outputs: only addresses, sizes and names from the reference lists.
import argparse
import bisect
import collections
import difflib
import hashlib
import struct
import sys
from pathlib import Path


class Dol:
    def __init__(self, path):
        d = Path(path).read_bytes()
        offs = struct.unpack(">18I", d[0x00:0x48])
        addrs = struct.unpack(">18I", d[0x48:0x90])
        sizes = struct.unpack(">18I", d[0x90:0xD8])
        self.text = []  # (addr, words)
        self.data = []  # (addr, bytes)
        for i in range(18):
            if not sizes[i]:
                continue
            blob = d[offs[i]:offs[i] + sizes[i]]
            if i < 7:
                n = len(blob) // 4
                self.text.append((addrs[i], struct.unpack(">%dI" % n, blob[:n * 4])))
            else:
                self.data.append((addrs[i], blob))
        self.entry = struct.unpack(">I", d[0xE0:0xE4])[0]

    def in_text(self, a):
        return any(base <= a < base + 4 * len(w) for base, w in self.text)


TERMINATORS = (0x4E800020, 0x4E800420, 0x4C000064)  # blr, bctr, rfi


def is_uncond_b(w):
    return (w >> 26) == 18 and not (w & 1)


def find_functions(dol):
    """Returns {entry address: [words]} with a simple boundary rule: bl targets, prologues after a terminator, code pointers in data."""
    entries = set()
    if dol.in_text(dol.entry):
        entries.add(dol.entry)
    for base, words in dol.text:
        prev_term = True
        for i, w in enumerate(words):
            a = base + 4 * i
            if (w >> 26) == 18 and (w & 1):  # bl
                disp = w & 0x03FFFFFC
                if disp & 0x02000000:
                    disp -= 0x04000000
                t = disp if (w & 2) else a + disp
                if dol.in_text(t):
                    entries.add(t)
            if prev_term and w != 0:
                # stwu r1,-x(r1) / mflr r0 start a function after a terminator
                if (w & 0xFFFF8000) == 0x94218000 or w == 0x7C0802A6:
                    entries.add(a)
            if w in TERMINATORS or is_uncond_b(w):
                prev_term = True
            elif w != 0:
                prev_term = False
    # pointers in data (virtual tables, callbacks) that point just after a terminator or padding
    word_at = {}
    for base, words in dol.text:
        for i, w in enumerate(words):
            word_at[base + 4 * i] = w
    for base, blob in dol.data:
        n = len(blob) // 4
        for v in struct.unpack(">%dI" % n, blob[:n * 4]):
            if (v & 3) == 0 and v in word_at and v not in entries:
                p = word_at.get(v - 4)
                if p is None or p == 0 or p in TERMINATORS or is_uncond_b(p):
                    entries.add(v)
    funcs = {}
    for base, words in dol.text:
        end = base + 4 * len(words)
        local = sorted(e for e in entries if base <= e < end)
        for k, e in enumerate(local):
            stop = local[k + 1] if k + 1 < len(local) else end
            body = list(words[(e - base) // 4:(stop - base) // 4])
            while body and body[-1] == 0:
                body.pop()
            if body:
                funcs[e] = body
    return funcs


D_FORM_BASE = set([14, 12, 13, 7, 8]) | set(range(32, 56))  # addi, addic, addic., mulli, subfic, loads and stores


def mask_function(words, addr):
    """Returns (masked words, relocation list). A relocation is (index, kind, value): kind 'bl' (target), 'sda13', 'sda2' (displacement),
    'hi' (lis immediate) or 'lo' (low half used with a lis register)."""
    out = []
    relocs = []
    lis_regs = set()
    for i, w in enumerate(words):
        op = w >> 26
        rd = (w >> 21) & 31
        ra = (w >> 16) & 31
        imm = w & 0xFFFF
        m = w
        if op == 18:
            disp = w & 0x03FFFFFC
            if disp & 0x02000000:
                disp -= 0x04000000
            t = disp if (w & 2) else addr + 4 * i + disp
            lo, hi = addr, addr + 4 * len(words)
            if (w & 1) or not (lo <= t < hi):
                m = w & 0xFC000003
                relocs.append((i, "bl" if (w & 1) else "b", t))
            lis_regs.clear() if (w & 1) else None
        elif op == 15 and ra == 0:  # lis
            m = w & 0xFFFF0000
            relocs.append((i, "hi", imm))
            lis_regs.add(rd)
        elif op in D_FORM_BASE and ra in (2, 13) and op not in (7, 8, 12, 13):
            m = w & 0xFFFF0000
            relocs.append((i, "sda13" if ra == 13 else "sda2", imm))
            if op == 14 or 32 <= op <= 35 or 40 <= op <= 43:
                lis_regs.discard(rd)
        elif op in D_FORM_BASE and ra in lis_regs and ra != 0:
            m = w & 0xFFFF0000
            relocs.append((i, "lo", imm))
            if op in (14, 12, 13, 7, 8) or 32 <= op <= 35 or 40 <= op <= 43:
                if rd != ra or op != 14:
                    lis_regs.discard(rd)
                # addi rX,rX,lo keeps nothing to track
                if op == 14 and rd == ra:
                    lis_regs.discard(rd)
        elif op in (24, 25) and rd in lis_regs:  # ori / oris rA <- rS(lis)
            m = w & 0xFFFF0000
            relocs.append((i, "lo", imm))
            lis_regs.discard(ra)
        else:
            # any other write to a tracked register ends the tracking
            if op in (14, 15, 12, 13, 7, 8) or 32 <= op <= 35 or 40 <= op <= 43:
                lis_regs.discard(rd)
            elif op in (24, 25, 26, 27, 28, 29):
                lis_regs.discard(ra)
            elif op == 31:
                lis_regs.discard(rd)
                lis_regs.discard(ra)
        out.append(m)
    return out, relocs


def signature(masked):
    return hashlib.sha1(struct.pack(">%dI" % len(masked), *masked)).digest()


def load_names(project):
    names = collections.defaultdict(list)  # addr -> [name]
    for fn in ("identified_sdk.txt", "symbols.txt", "gx_symbols.txt"):
        p = project / fn
        if not p.exists():
            continue
        for line in p.read_text(encoding="utf-8").splitlines():
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                try:
                    names[int(parts[0], 16)].append(parts[1])
                except ValueError:
                    pass
    hle_addr = []  # (addr, hle name, comment)
    p = project / "hle_map.txt"
    if p.exists():
        for raw in p.read_text(encoding="utf-8").splitlines():
            line = raw.split("#", 1)[0].strip()
            if line.startswith("@"):
                parts = line.split()
                hle_addr.append((int(parts[0][1:], 16), parts[1]))
    return names, hle_addr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", required=True)
    ap.add_argument("--new", required=True)
    ap.add_argument("--project", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--min-words", type=int, default=4)
    args = ap.parse_args()

    ref, new = Dol(args.ref), Dol(args.new)
    rf, nf = find_functions(ref), find_functions(new)
    print("functions: ref %d, new %d" % (len(rf), len(nf)))

    rmask = {a: mask_function(w, a) for a, w in rf.items()}
    nmask = {a: mask_function(w, a) for a, w in nf.items()}
    rsig = collections.defaultdict(list)
    nsig = collections.defaultdict(list)
    for a, (m, _) in rmask.items():
        rsig[signature(m)].append(a)
    for a, (m, _) in nmask.items():
        nsig[signature(m)].append(a)

    match = {}  # ref addr -> (new addr, method)
    taken = {}  # new addr -> ref addr
    queue = collections.deque()

    def add(r, n, method):
        if r in match or n in taken or r not in rf or n not in nf:
            return False
        match[r] = (n, method)
        taken[n] = r
        queue.append((r, n))
        return True

    for s, ras in rsig.items():
        nas = nsig.get(s)
        if nas and len(ras) == 1 and len(nas) == 1 and len(rf[ras[0]]) >= args.min_words:
            add(ras[0], nas[0], "exact")
    n_exact = len(match)

    sda = collections.Counter()
    data = collections.Counter()
    conflicts = 0

    def propagate():
        nonlocal conflicts
        while queue:
            r, n = queue.popleft()
            (rm, rr), (nm, nr) = rmask[r], nmask[n]
            if rm != nm:
                # matched by call or order: line up the parts of the two bodies that are still the same
                if len(rm) > 4000 or len(nm) > 4000:
                    continue
                sm = difflib.SequenceMatcher(None, rm, nm, autojunk=False)
                if sm.quick_ratio() < 0.5:
                    continue
                shift = {}
                for b in sm.get_matching_blocks():
                    if b.size >= 3:
                        for k in range(b.size):
                            shift[b.a + k] = b.b + k
                nrel = {j: (kind2, nv) for j, kind2, nv in nr}
                pairs = [((i, kind, rv), (shift[i],) + nrel[shift[i]]) for i, kind, rv in rr if i in shift and shift[i] in nrel]
            else:
                pairs = list(zip(rr, nr))
            hi_r = hi_n = None
            for (i, kind, rv), (j, kind2, nv) in pairs:
                if kind != kind2:
                    continue
                if kind == "bl":
                    if rv in match:
                        if match[rv][0] != nv:
                            conflicts += 1
                    else:
                        add(rv, nv, "call")
                elif kind in ("sda13", "sda2"):
                    sda[(kind, rv, nv)] += 1
                elif kind == "hi":
                    hi_r, hi_n = rv, nv
                elif kind == "lo" and hi_r is not None:
                    def full(h, l):
                        return ((h << 16) + (l - 0x10000 if l & 0x8000 else l)) & 0xFFFFFFFF
                    data[(full(hi_r, rv), full(hi_n, nv))] += 1

    propagate()
    n_call = len(match) - n_exact

    # order: runs of unmatched functions between two matched neighbours with equal counts and sizes
    rkeys, nkeys = sorted(rf), sorted(nf)
    rpos = {a: i for i, a in enumerate(rkeys)}
    npos = {a: i for i, a in enumerate(nkeys)}
    changed = True
    n_order = 0
    while changed:
        changed = False
        anchors = sorted((rpos[r], npos[n]) for r, (n, _) in match.items())
        for (r0, n0), (r1, n1) in zip(anchors, anchors[1:]):
            gap = r1 - r0
            if gap < 2 or gap != n1 - n0 or gap > 40:
                continue
            ok = all(len(rf[rkeys[r0 + k]]) == len(nf[nkeys[n0 + k]]) for k in range(1, gap))
            if not ok and gap <= 7:
                # a short run of changed functions between two fixed neighbours: accept sizes that are close
                ok = all(0.6 <= len(nf[nkeys[n0 + k]]) / len(rf[rkeys[r0 + k]]) <= 1.6 for k in range(1, gap))
            if not ok:
                continue
            for k in range(1, gap):
                if add(rkeys[r0 + k], nkeys[n0 + k], "order"):
                    n_order += 1
                    changed = True
        propagate()

    print("matched: exact %d, call %d, order %d, total %d of %d ref functions (%d call conflicts)" % (
        n_exact, len(match) - n_exact - n_order, n_order, len(match), len(rf), conflicts))

    names, hle_addr = load_names(Path(args.project))

    def inside(r):
        # an address inside a ref function whose body is the same in the new program moves by the same offset
        i = bisect.bisect_right(rkeys, r) - 1
        if i < 0:
            return None
        R = rkeys[i]
        if R == r or R not in match or r >= R + 4 * len(rf[R]):
            return None
        N = match[R][0]
        if rmask[R][0] != nmask[N][0]:
            return None
        return N + (r - R)

    for r in list(names) + [a for a, _ in hle_addr]:
        if r not in match:
            n = inside(r)
            if n is not None:
                match[r] = (n, "inside")

    # pattern: a wanted address that is still missing may be a routine without a prologue that nothing calls with bl (a tail-called
    # leaf, a resume entry). Take its words up to the first blr (6 to 64 words), mask the relocations as above, and accept a place in
    # the new program where the same run stands exactly once.
    def run_at(dol, addr):
        for base, words in dol.text:
            if base <= addr < base + 4 * len(words):
                k = (addr - base) // 4
                out = []
                while k < len(words) and len(out) < 64:
                    out.append(words[k])
                    if words[k] == 0x4E800020:
                        break
                    k += 1
                return out
        return []

    new_masked = [(base, [mask_function([w], base + 4 * i)[0][0] for i, w in enumerate(words)]) for base, words in new.text]
    first_index = collections.defaultdict(list)
    for base, masked in new_masked:
        for i, w in enumerate(masked):
            first_index[w].append((base, i))
    n_pattern = 0
    for r in sorted(set(list(names) + [a for a, _ in hle_addr])):
        if r in match:
            continue
        run = run_at(ref, r)
        if len(run) < 6 or run[-1] != 0x4E800020:
            continue
        masked = [mask_function([w], r + 4 * i)[0][0] for i, w in enumerate(run)]
        hits = []
        for base, i in first_index.get(masked[0], ()):
            seg = next(m for b, m in new_masked if b == base)[i:i + len(masked)]
            if seg == masked:
                hits.append(base + 4 * i)
        if len(hits) == 1 and hits[0] not in taken:
            match[r] = (hits[0], "pattern")
            taken[hits[0]] = r
            n_pattern += 1
    print("pattern: %d more addresses" % n_pattern)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    lines = []
    for r in sorted(match, key=lambda a: match[a][0]):
        n, method = match[r]
        nm = names.get(r, [""])[0]
        lines.append("%08X %08X %s %d %s" % (n, r, method, len(nf.get(n, ())), nm))
    same = ["# Functions that are the same code in both programs (relocations masked): <ref start> <new start> <size words>.",
            "# An address inside such a function moves by the same offset (tools/title_map.py)."]
    for r in sorted(match):
        n, method = match[r]
        if method != "inside" and r in rmask and n in nmask and rmask[r][0] == nmask[n][0]:
            same.append("%08X %08X %d" % (r, n, len(rf[r])))
    (out / "same_code.txt").write_text("\n".join(same) + "\n", encoding="utf-8")
    (out / "matched_functions.txt").write_text("# <new addr> <ref addr> <method> <size words> [name]\n" + "\n".join(lines) + "\n", encoding="utf-8")

    sym = []
    named_total = named_found = 0
    for r, nms in sorted(names.items()):
        named_total += 1
        if r in match:
            named_found += 1
            n, method = match[r]
            for nm in nms:
                sym.append("%08X %s carried %s from %08X" % (n, nm, method, r))
    (out / "carried_symbols.txt").write_text("# Names carried from the reference project by tools/dolmatch.py. Format: <addr> <name> carried <method> from <ref addr>\n" + "\n".join(sorted(sym)) + "\n", encoding="utf-8")

    hl = []
    hle_found = 0
    for r, nm in hle_addr:
        if r in match:
            hle_found += 1
            hl.append("@%08X %s  # %s from %08X" % (match[r][0], nm, match[r][1], r))
        else:
            hl.append("# missing %s (ref @%08X%s)" % (nm, r, "" if r in rf else ", not a function entry in this tool's view"))
    (out / "carried_hle_addresses.txt").write_text("\n".join(hl) + "\n", encoding="utf-8")

    sl = ["# <base> <ref displacement> <new displacement> <votes>"]
    best = collections.defaultdict(list)
    for (kind, rv, nv), c in sda.items():
        best[(kind, rv)].append((c, nv))
    for (kind, rv), lst in sorted(best.items()):
        lst.sort(reverse=True)
        sl.append("%s %04X %s" % (kind, rv, " ".join("%04X:%d" % (nv, c) for c, nv in lst[:3])))
    (out / "sda_offsets.txt").write_text("\n".join(sl) + "\n", encoding="utf-8")

    dl = ["# <ref address> <new address> <votes>"]
    bestd = collections.defaultdict(list)
    for (rv, nv), c in data.items():
        bestd[rv].append((c, nv))
    for rv, lst in sorted(bestd.items()):
        lst.sort(reverse=True)
        dl.append("%08X %s" % (rv, " ".join("%08X:%d" % (nv, c) for c, nv in lst[:3])))
    (out / "data_addresses.txt").write_text("\n".join(dl) + "\n", encoding="utf-8")

    print("names: %d of %d named ref addresses carried; hle @addresses: %d of %d" % (named_found, named_total, hle_found, len(hle_addr)))


if __name__ == "__main__":
    main()
