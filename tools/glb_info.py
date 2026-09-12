#!/usr/bin/env python3
"""Prints the material and mesh table of a .glb, straight from its JSON chunk.

The renderer shades what the GLB contains, not what the .ts source says, so this
is the file to read when a model looks wrong on screen but right in the source.

    <python> tools/glb_info.py assets/kestrel.glb
"""
import json
import struct
import sys
import pathlib


def chunks(path):
    data = pathlib.Path(path).read_bytes()
    magic, version, _total = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67:
        raise SystemExit("{}: not a glb".format(path))
    offset = 12
    out = {}
    while offset < len(data):
        length, kind = struct.unpack_from("<II", data, offset)
        offset += 8
        out[kind] = data[offset:offset + length]
        offset += length
    return out


def main():
    for arg in sys.argv[1:]:
        js = json.loads(chunks(arg)[0x4E4F534A].decode("utf-8"))
        name = pathlib.Path(arg).stem
        mats = js.get("materials", [])
        meshes = js.get("meshes", [])
        prims = sum(len(m.get("primitives", [])) for m in meshes)
        tris = 0
        for m in meshes:
            for p in m["primitives"]:
                # Non-indexed primitives are legal and common out of a merge step, so
                # falling back to the position count is not optional.
                if "indices" in p:
                    tris += js["accessors"][p["indices"]]["count"] // 3
                else:
                    tris += js["accessors"][p["attributes"]["POSITION"]]["count"] // 3
        print("{}  meshes {}  primitives {}  triangles {}  materials {}".format(
            name, len(meshes), prims, tris, len(mats)))
        for i, mat in enumerate(mats):
            pbr = mat.get("pbrMetallicRoughness", {})
            base = pbr.get("baseColorFactor", [1, 1, 1, 1])
            hexc = "#%02X%02X%02X" % tuple(
                min(255, max(0, round((c ** (1 / 2.2)) * 255))) for c in base[:3])
            raw = "#%02X%02X%02X" % tuple(
                min(255, max(0, round(c * 255))) for c in base[:3])
            print("  [{:2}] {:<18} srgb {}  linear {}  metal {:.2f}  rough {:.2f}{}".format(
                i, mat.get("name", "?")[:18], hexc, raw,
                pbr.get("metallicFactor", 1.0), pbr.get("roughnessFactor", 1.0),
                "  UNLIT" if "KHR_materials_unlit" in mat.get("extensions", {}) else ""))
        print()


if __name__ == "__main__":
    main()
