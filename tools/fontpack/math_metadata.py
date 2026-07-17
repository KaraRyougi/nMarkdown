#!/usr/bin/env python3
"""Extract OpenType MATH constants or serialize bounded tuned fallbacks."""

from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys


MAGIC = b"NMMATH1\0"
VERSION = 1
NAMES = (
    "axis_height",
    "fraction_rule",
    "fraction_num_gap",
    "fraction_den_gap",
    "superscript_shift",
    "subscript_shift",
    "radical_rule",
    "radical_gap",
)
MATH_FIELDS = {
    "axis_height": "AxisHeight",
    "fraction_rule": "FractionRuleThickness",
    "fraction_num_gap": "FractionNumeratorGapMin",
    "fraction_den_gap": "FractionDenominatorGapMin",
    "superscript_shift": "SuperscriptShiftUp",
    "subscript_shift": "SubscriptShiftDown",
    "radical_rule": "RadicalRuleThickness",
    "radical_gap": "RadicalVerticalGap",
}
HEADER_FORMAT = "<8sHH8iI"
CHECKSUM_OFFSET = struct.calcsize("<8sHH8i")


def fnv1a(data: bytes, zero_checksum: bool = False) -> int:
    value = 2166136261
    for index, byte in enumerate(data):
        if zero_checksum and CHECKSUM_OFFSET <= index < CHECKSUM_OFFSET + 4:
            byte = 0
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def defaults(path: pathlib.Path) -> dict[str, int]:
    raw = json.loads(path.read_text(encoding="utf-8"))
    result: dict[str, int] = {}
    for name in NAMES:
        value = float(raw[name])
        if not -4.0 <= value <= 4.0:
            raise ValueError(f"{name} is outside the bounded em range")
        result[name] = round(value * 65536)
    return result


def extract(font_path: pathlib.Path, fallback: dict[str, int]) -> tuple[dict[str, int], str]:
    try:
        from fontTools.ttLib import TTFont
    except ImportError:
        return fallback, "tuned-fallback (fontTools unavailable)"

    with TTFont(font_path, lazy=True) as font:
        if "MATH" not in font:
            return fallback, "tuned-fallback (font has no OpenType MATH table)"
        units_per_em = int(font["head"].unitsPerEm)
        constants = font["MATH"].table.MathConstants
        result = dict(fallback)
        for name, field in MATH_FIELDS.items():
            record = getattr(constants, field, None)
            value = getattr(record, "Value", record)
            if value is not None and int(value) > 0:
                result[name] = round(int(value) * 65536 / units_per_em)
        return result, "OpenType MATH"


def build_binary(values: dict[str, int]) -> bytes:
    size = struct.calcsize(HEADER_FORMAT)
    data = bytearray(struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        size,
        *(values[name] for name in NAMES),
        0,
    ))
    struct.pack_into("<I", data, CHECKSUM_OFFSET, fnv1a(data, True))
    return bytes(data)


def build_cpp(values: dict[str, int], provenance: str) -> str:
    rows = [
        '#include "nmarkdown/generated/core_math_font.h"',
        "",
        "namespace nmarkdown {",
        "",
        f"// Generated from {provenance}.",
        "const MathFontConstants kCoreMathFontConstants = {",
    ]
    rows.extend(f"    {values[name]}," for name in NAMES)
    rows.extend(["};", "", "}  // namespace nmarkdown", ""])
    return "\n".join(rows)


def write_if_changed(path: pathlib.Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=pathlib.Path)
    parser.add_argument("--defaults", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--cpp-output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    try:
        values, provenance = extract(args.font, defaults(args.defaults))
        binary = build_binary(values)
        write_if_changed(args.output, binary)
        write_if_changed(args.cpp_output, build_cpp(values, provenance).encode("utf-8"))
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"math-metadata: {error}", file=sys.stderr)
        return 1
    print(
        f"math-metadata: wrote {len(binary)} bytes from {provenance}, "
        f"checksum 0x{fnv1a(binary, True):08x}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
