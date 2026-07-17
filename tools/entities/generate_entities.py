#!/usr/bin/env python3
"""Generate the compact CommonMark/HTML5 named-entity lookup table."""

from __future__ import annotations

import argparse
import html.entities
import pathlib


def byte_array(name: str, data: bytes) -> list[str]:
    lines = [f"const unsigned char {name}[] = {{"]
    for begin in range(0, len(data), 16):
        chunk = data[begin : begin + 16]
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    lines.append("};")
    return lines


def generate() -> str:
    entries = sorted(
        (name[:-1], value)
        for name, value in html.entities.html5.items()
        if name.endswith(";")
    )
    names = bytearray()
    values = bytearray()
    value_locations: dict[bytes, tuple[int, int]] = {}
    records: list[tuple[int, int, int, int]] = []
    for name, value in entries:
        name_bytes = name.encode("ascii")
        value_bytes = value.encode("utf-8")
        name_offset = len(names)
        names.extend(name_bytes)
        if value_bytes not in value_locations:
            value_locations[value_bytes] = (len(values), len(value_bytes))
            values.extend(value_bytes)
        value_offset, value_size = value_locations[value_bytes]
        records.append((name_offset, len(name_bytes), value_offset, value_size))

    lines = [
        '#include "nmarkdown/generated/html_entities.h"',
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace nmarkdown {",
        "namespace {",
        "",
        "struct EntityRecord {",
        "    std::uint32_t name_offset;",
        "    std::uint32_t value_offset;",
        "    std::uint16_t name_size;",
        "    std::uint8_t value_size;",
        "};",
        "",
    ]
    lines.extend(byte_array("kEntityNames", bytes(names)))
    lines.append("")
    lines.extend(byte_array("kEntityValues", bytes(values)))
    lines.extend(["", "const EntityRecord kEntityRecords[] = {"])
    for name_offset, name_size, value_offset, value_size in records:
        lines.append(
            f"    {{{name_offset}U, {value_offset}U, {name_size}U, {value_size}U}},"
        )
    lines.extend(
        [
            "};",
            "",
            "std::string_view entity_name(const EntityRecord& record) {",
            "    return {reinterpret_cast<const char*>(kEntityNames + record.name_offset),",
            "            record.name_size};",
            "}",
            "",
            "}  // namespace",
            "",
            "bool lookup_html_entity(std::string_view name, std::string_view& utf8_value) {",
            "    std::size_t first = 0;",
            "    std::size_t last = sizeof(kEntityRecords) / sizeof(kEntityRecords[0]);",
            "    while (first < last) {",
            "        const std::size_t middle = first + (last - first) / 2;",
            "        const std::string_view candidate = entity_name(kEntityRecords[middle]);",
            "        if (candidate < name) {",
            "            first = middle + 1;",
            "        } else {",
            "            last = middle;",
            "        }",
            "    }",
            "    if (first >= sizeof(kEntityRecords) / sizeof(kEntityRecords[0]) ||",
            "        entity_name(kEntityRecords[first]) != name) {",
            "        utf8_value = {};",
            "        return false;",
            "    }",
            "    const EntityRecord& record = kEntityRecords[first];",
            "    utf8_value = {reinterpret_cast<const char*>(kEntityValues + record.value_offset),",
            "                  record.value_size};",
            "    return true;",
            "}",
            "",
            "}  // namespace nmarkdown",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    content = generate()
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != content:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content, encoding="utf-8")
    print(f"entities: wrote {len(html.entities.html5)} HTML5 aliases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

