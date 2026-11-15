"""Merges pipeline key files (renderer/pipeline_keys.h: a header of three words, then keys of 70 words) into one file without duplicates.

  python -I tools/merge_pipeline_keys.py --out project/dc/pipeline_keys.seed KEYFILE...

The renderer records every new key in data/pipeline_keys.bin beside the executable (on the headset: in the app folder). The lists of a
game in project/ come from such records: the build of the headset folds a TEV module for each key (tools/fold_tev_shaders.py), and the
renderer compiles the listed keys at start. A key file that does not exist is skipped. The output keeps the keys it already has.
"""
import argparse
import os
import struct

HEADER = struct.pack("<3I", 0x4B504355, 1, 280)
SIZE = 280


def read(path):
    if not os.path.exists(path):
        return []
    data = open(path, "rb").read()
    if data[:12] != HEADER:
        raise SystemExit("%s is not a pipeline key file" % path)
    return [data[i:i + SIZE] for i in range(12, len(data) - SIZE + 1, SIZE)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("keys", nargs="+")
    args = parser.parse_args()
    merged, seen = [], set()
    for path in [args.out] + args.keys:
        added = 0
        for key in read(path):
            if key not in seen:
                seen.add(key)
                merged.append(key)
                added += 1
        print("%s: %d new keys" % (path, added))
    with open(args.out + ".tmp", "wb") as out:
        out.write(HEADER + b"".join(merged))
    os.replace(args.out + ".tmp", args.out)
    print("%s: %d keys" % (args.out, len(merged)))


if __name__ == "__main__":
    main()
