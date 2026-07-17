#!/usr/bin/env python3
"""Generate compact Unicode case-folding and canonical-normalization tables."""

from __future__ import annotations

import argparse
import pathlib
import urllib.request


FILES = ("UnicodeData.txt", "CaseFolding.txt", "DerivedNormalizationProps.txt")


def load_ucd(version: str, directory: pathlib.Path | None) -> dict[str, str]:
    if directory:
        return {name: (directory / name).read_text(encoding="utf-8") for name in FILES}
    base = f"https://www.unicode.org/Public/{version}/ucd/"
    result: dict[str, str] = {}
    for name in FILES:
        with urllib.request.urlopen(base + name, timeout=30) as response:
            result[name] = response.read().decode("utf-8")
    return result


def codepoint_range(value: str) -> range:
    parts = value.strip().split("..")
    first = int(parts[0], 16)
    last = int(parts[-1], 16)
    return range(first, last + 1)


def parse_unicode_data(text: str) -> tuple[dict[int, list[int]], dict[int, int]]:
    decompositions: dict[int, list[int]] = {}
    classes: dict[int, int] = {}
    for line in text.splitlines():
        if not line:
            continue
        fields = line.split(";")
        codepoint = int(fields[0], 16)
        combining = int(fields[3])
        if combining:
            classes[codepoint] = combining
        raw = fields[5]
        if raw and not raw.startswith("<"):
            decompositions[codepoint] = [int(value, 16) for value in raw.split()]
    return decompositions, classes


def parse_case_folding(text: str) -> dict[int, list[int]]:
    simple: dict[int, list[int]] = {}
    full: dict[int, list[int]] = {}
    for raw_line in text.splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        codepoint_text, status, mapping, *_ = [part.strip() for part in line.split(";")]
        codepoint = int(codepoint_text, 16)
        values = [int(value, 16) for value in mapping.split()]
        if status == "C":
            simple[codepoint] = values
        elif status == "F":
            full[codepoint] = values
    simple.update(full)
    return simple


def parse_composition_exclusions(text: str) -> set[int]:
    excluded: set[int] = set()
    for raw_line in text.splitlines():
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        fields = [part.strip() for part in line.split(";")]
        if len(fields) >= 2 and fields[1] == "Full_Composition_Exclusion":
            excluded.update(codepoint_range(fields[0]))
    return excluded


def flatten_mappings(mapping: dict[int, list[int]]) -> tuple[list[tuple[int, int, int]], list[int]]:
    records: list[tuple[int, int, int]] = []
    values: list[int] = []
    for codepoint, mapped in sorted(mapping.items()):
        records.append((codepoint, len(values), len(mapped)))
        values.extend(mapped)
    return records, values


def compositions(
    decompositions: dict[int, list[int]], excluded: set[int], classes: dict[int, int]
) -> list[tuple[int, int, int]]:
    result = [
        (mapped[0], mapped[1], composed)
        for composed, mapped in decompositions.items()
        if len(mapped) == 2 and composed not in excluded and classes.get(mapped[0], 0) == 0
    ]
    result.sort()
    return result


def emit_u32_array(name: str, values: list[int]) -> list[str]:
    lines = [f"const std::uint32_t {name}[] = {{"]
    for begin in range(0, len(values), 10):
        lines.append(
            "    "
            + ", ".join(f"0x{value:X}U" for value in values[begin : begin + 10])
            + ","
        )
    lines.extend(
        ["};", f"const std::size_t {name}Count = sizeof({name}) / sizeof({name}[0]);", ""]
    )
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--unicode-version", default="17.0.0")
    parser.add_argument("--ucd-dir", type=pathlib.Path)
    args = parser.parse_args()

    data = load_ucd(args.unicode_version, args.ucd_dir)
    decomposition_map, combining_classes = parse_unicode_data(data["UnicodeData.txt"])
    fold_map = parse_case_folding(data["CaseFolding.txt"])
    exclusions = parse_composition_exclusions(data["DerivedNormalizationProps.txt"])
    fold_records, fold_values = flatten_mappings(fold_map)
    decomposition_records, decomposition_values = flatten_mappings(decomposition_map)
    classes = sorted(combining_classes.items())
    compose = compositions(decomposition_map, exclusions, combining_classes)

    lines = [
        '#include "nmarkdown/generated/unicode_tables.h"',
        "",
        "namespace nmarkdown {",
        "",
        f'const char kUnicodeDataVersion[] = "{args.unicode_version}";',
        "",
    ]
    for name, records in (
        ("kUnicodeCaseFoldRecords", fold_records),
        ("kUnicodeDecompositionRecords", decomposition_records),
    ):
        lines.append(f"const UnicodeMappingRecord {name}[] = {{")
        lines.extend(
            f"    {{0x{codepoint:X}U, {offset}U, {length}}},"
            for codepoint, offset, length in records
        )
        lines.extend(
            ["};", f"const std::size_t {name}Count = sizeof({name}) / sizeof({name}[0]);", ""]
        )
    lines.extend(emit_u32_array("kUnicodeCaseFoldValues", fold_values))
    lines.extend(emit_u32_array("kUnicodeDecompositionValues", decomposition_values))
    lines.append("const UnicodeClassRecord kUnicodeCombiningClasses[] = {")
    lines.extend(f"    {{0x{codepoint:X}U, {value}}}," for codepoint, value in classes)
    lines.extend(
        [
            "};",
            "const std::size_t kUnicodeCombiningClassesCount = "
            "sizeof(kUnicodeCombiningClasses) / sizeof(kUnicodeCombiningClasses[0]);",
            "",
            "const UnicodeCompositionRecord kUnicodeCompositions[] = {",
        ]
    )
    lines.extend(
        f"    {{0x{first:X}U, 0x{second:X}U, 0x{composed:X}U}},"
        for first, second, composed in compose
    )
    lines.extend(
        [
            "};",
            "const std::size_t kUnicodeCompositionsCount = "
            "sizeof(kUnicodeCompositions) / sizeof(kUnicodeCompositions[0]);",
            "",
            "}  // namespace nmarkdown",
            "",
        ]
    )
    output = "\n".join(lines).encode("utf-8")
    if not args.output.exists() or args.output.read_bytes() != output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(output)
    print(
        f"unicode: {args.unicode_version}, {len(fold_records)} folds, "
        f"{len(decomposition_records)} decompositions, {len(classes)} classes, "
        f"{len(compose)} compositions"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
