#!/usr/bin/env python3
"""Lists where things stand in a DK64 map, from its setup file, to place new content on
solid ground (a colored banana's spot is somewhere the player can reach).

    build/jiggy-inspect extract <dk64-rom> 9:<map> setup.bin
    tools/dk64-setup/setup_dump.py setup.bin [--near X Z RADIUS] [--types]

Prints model2 objects (bananas, coins, crates, scenery: x y z scale type) and actor spawners
(x y z type). Setup layout from the DK64 decompilation: u32 model2 count, 0x30-byte entries
(f32 x, y, z, scale at 0x00; u16 type at 0x28); u32 count of 0x24-byte entries; u32 actor
count, 0x38-byte entries (f32 x, y, z at 0x00; u16 type at 0x32, +0x10 for the actor id).
The setup comes from the player's own ROM: don't commit what this prints in bulk.
"""
import argparse
import math
import struct


def parse(data):
    off = 0
    (n,) = struct.unpack_from(">I", data, off)
    off += 4
    model2 = []
    for i in range(n):
        x, y, z, scale = struct.unpack_from(">4f", data, off)
        (kind,) = struct.unpack_from(">H", data, off + 0x28)
        model2.append((x, y, z, scale, kind))
        off += 0x30
    (n,) = struct.unpack_from(">I", data, off)
    off += 4 + n * 0x24
    actors = []
    if off + 4 <= len(data):
        (n,) = struct.unpack_from(">I", data, off)
        off += 4
        for i in range(n):
            if off + 0x38 > len(data):
                break
            x, y, z = struct.unpack_from(">3f", data, off)
            (kind,) = struct.unpack_from(">H", data, off + 0x32)
            actors.append((x, y, z, kind + 0x10))
            off += 0x38
    return model2, actors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("setup")
    ap.add_argument("--near", nargs=3, type=float, metavar=("X", "Z", "R"))
    ap.add_argument("--types", action="store_true", help="only count objects per type")
    args = ap.parse_args()
    model2, actors = parse(open(args.setup, "rb").read())

    def keep(x, z):
        if not args.near:
            return True
        return math.hypot(x - args.near[0], z - args.near[1]) <= args.near[2]

    if args.types:
        counts = {}
        for x, y, z, s, k in model2:
            counts[("model2", k)] = counts.get(("model2", k), 0) + 1
        for x, y, z, k in actors:
            counts[("actor", k)] = counts.get(("actor", k), 0) + 1
        for (what, k), c in sorted(counts.items()):
            print(f"{what} 0x{k:X}: {c}")
        return
    for x, y, z, s, k in model2:
        if keep(x, z):
            print(f"model2 {x:.0f} {y:.0f} {z:.0f} scale {s:.2f} type 0x{k:X}")
    for x, y, z, k in actors:
        if keep(x, z):
            print(f"actor {x:.0f} {y:.0f} {z:.0f} type 0x{k:X}")


if __name__ == "__main__":
    main()
