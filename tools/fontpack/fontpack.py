#!/usr/bin/env python3
"""Build the nMarkdown versioned font-pack binary and optional C++ blob."""

from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys
from dataclasses import dataclass


MAGIC = b"NMFONT1\0"
VERSION = 1
HEADER_FORMAT = "<8sHH10I"
FACE_FORMAT = "<10I"
RANGE_FORMAT = "<2I"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
FACE_SIZE = struct.calcsize(FACE_FORMAT)
RANGE_SIZE = struct.calcsize(RANGE_FORMAT)
CHECKSUM_OFFSET = 44

ROLE_IDS = {
    "body-sans": 1,
    "body-serif": 2,
    "monospace": 3,
    "math": 4,
    "cjk": 5,
    "body-sans-italic": 7,
    "replacement": 255,
}


@dataclass
class Face:
    face_id: int
    role: int
    name: str
    font: bytes
    license_text: str
    ranges: list[tuple[int, int]]


def parse_codepoint(value: str | int) -> int:
    if isinstance(value, int):
        result = value
    else:
        normalized = value.strip().upper()
        if normalized.startswith("U+"):
            normalized = normalized[2:]
        result = int(normalized, 16)
    if result < 0 or result > 0x10FFFF:
        raise ValueError(f"codepoint out of range: {value}")
    return result


def align4(buffer: bytearray) -> None:
    while len(buffer) % 4:
        buffer.append(0)


def fnv1a_with_zeroed_checksum(data: bytes | bytearray) -> int:
    value = 2166136261
    for index, byte in enumerate(data):
        if CHECKSUM_OFFSET <= index < CHECKSUM_OFFSET + 4:
            byte = 0
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def load_manifest(path: pathlib.Path) -> list[Face]:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if raw.get("version") != VERSION:
        raise ValueError(f"manifest version must be {VERSION}")
    faces: list[Face] = []
    seen_ids: set[int] = set()
    for item in raw.get("faces", []):
        face_id = int(item["id"])
        if face_id <= 0 or face_id in seen_ids:
            raise ValueError(f"invalid or duplicate face id: {face_id}")
        seen_ids.add(face_id)
        role_name = item["role"]
        if role_name not in ROLE_IDS:
            raise ValueError(f"unknown role: {role_name}")

        ranges: list[tuple[int, int]] = []
        for raw_range in item.get("ranges", []):
            if len(raw_range) != 2:
                raise ValueError(f"range must have two endpoints: {raw_range}")
            start = parse_codepoint(raw_range[0])
            end = parse_codepoint(raw_range[1])
            if start > end:
                raise ValueError(f"range starts after it ends: {raw_range}")
            ranges.append((start, end))
        ranges.sort()
        for previous, current in zip(ranges, ranges[1:]):
            if current[0] <= previous[1]:
                raise ValueError(f"overlapping ranges: {previous} and {current}")

        font_path = path.parent / item["font"]
        license_path = path.parent / item["license"]
        # Keep the license path in the source manifest so a missing notice is a
        # build error.  Core packs do not need to duplicate that text: the
        # distributable notices live next to the application and the runtime
        # never displays font licenses.  Older manifests may opt in to the
        # version-1 license field for compatibility.
        license_text = license_path.read_text(encoding="utf-8")
        if not item.get("embed_license", True):
            license_text = ""
        faces.append(
            Face(
                face_id=face_id,
                role=ROLE_IDS[role_name],
                name=item["name"],
                font=font_path.read_bytes(),
                license_text=license_text,
                ranges=ranges,
            )
        )
    if not faces:
        raise ValueError("manifest must contain at least one font face")
    return faces


def build_pack(faces: list[Face]) -> bytes:
    face_table_offset = HEADER_SIZE
    range_table_offset = face_table_offset + len(faces) * FACE_SIZE
    all_ranges = [item for face in faces for item in face.ranges]
    string_table_offset = range_table_offset + len(all_ranges) * RANGE_SIZE

    strings = bytearray()
    string_locations: list[tuple[int, int, int, int]] = []
    for face in faces:
        name = face.name.encode("utf-8")
        license_bytes = face.license_text.encode("utf-8")
        name_offset = len(strings)
        strings.extend(name)
        license_offset = len(strings)
        strings.extend(license_bytes)
        string_locations.append(
            (name_offset, len(name), license_offset, len(license_bytes))
        )

    payload_offset = string_table_offset + len(strings)
    payload_padding = (-payload_offset) % 4
    payload_offset += payload_padding

    font_offsets: list[int] = []
    cursor = payload_offset
    for face in faces:
        font_offsets.append(cursor)
        cursor += len(face.font)
        cursor = (cursor + 3) & ~3
    file_size = cursor

    buffer = bytearray(file_size)
    buffer[string_table_offset : string_table_offset + len(strings)] = strings

    range_cursor = 0
    for index, face in enumerate(faces):
        name_offset, name_size, license_offset, license_size = string_locations[index]
        record = struct.pack(
            FACE_FORMAT,
            face.face_id,
            face.role,
            font_offsets[index],
            len(face.font),
            name_offset,
            name_size,
            license_offset,
            license_size,
            range_cursor,
            len(face.ranges),
        )
        begin = face_table_offset + index * FACE_SIZE
        buffer[begin : begin + FACE_SIZE] = record
        for value_range in face.ranges:
            range_record = struct.pack(RANGE_FORMAT, *value_range)
            range_begin = range_table_offset + range_cursor * RANGE_SIZE
            buffer[range_begin : range_begin + RANGE_SIZE] = range_record
            range_cursor += 1
        font_begin = font_offsets[index]
        buffer[font_begin : font_begin + len(face.font)] = face.font

    header = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        VERSION,
        HEADER_SIZE,
        file_size,
        len(faces),
        face_table_offset,
        range_table_offset,
        len(all_ranges),
        string_table_offset,
        len(strings),
        payload_offset,
        0,
        0,
    )
    buffer[:HEADER_SIZE] = header
    checksum = fnv1a_with_zeroed_checksum(buffer)
    struct.pack_into("<I", buffer, CHECKSUM_OFFSET, checksum)
    return bytes(buffer)


def cpp_blob(data: bytes, symbol: str, header: str) -> str:
    lines = [f'#include "{header}"', "", "namespace nmarkdown {", ""]
    lines.append(f"alignas(4) const std::uint8_t {symbol}[] = {{")
    for begin in range(0, len(data), 16):
        chunk = data[begin : begin + 16]
        lines.append("    " + ", ".join(f"0x{byte:02X}" for byte in chunk) + ",")
    lines.extend(
        [
            "};",
            f"const std::size_t {symbol}Size = sizeof({symbol});",
            "",
            "}  // namespace nmarkdown",
            "",
        ]
    )
    return "\n".join(lines)


def write_if_changed(path: pathlib.Path, data: bytes) -> None:
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--cpp-output", type=pathlib.Path)
    parser.add_argument("--symbol", default="kCoreFontPack")
    parser.add_argument(
        "--header", default="nmarkdown/generated/core_font_pack.h"
    )
    args = parser.parse_args()

    try:
        faces = load_manifest(args.manifest)
        pack = build_pack(faces)
        write_if_changed(args.output, pack)
        if args.cpp_output:
            source = cpp_blob(pack, args.symbol, args.header).encode("utf-8")
            write_if_changed(args.cpp_output, source)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"fontpack: {error}", file=sys.stderr)
        return 1

    print(
        f"fontpack: wrote {len(pack)} bytes, {len(faces)} face(s), "
        f"checksum 0x{fnv1a_with_zeroed_checksum(pack):08x}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
