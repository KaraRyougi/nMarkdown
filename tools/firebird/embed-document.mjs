#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

const [sourceArgument, outputArgument, identifierArgument] = process.argv.slice(2);
if (!sourceArgument || !outputArgument) {
  console.error(
    "Usage: node tools/firebird/embed-document.mjs SOURCE.md OUTPUT.h " +
    "[IDENTIFIER]",
  );
  process.exit(2);
}

const identifier = identifierArgument ?? "MarkdownFormula";
if (!/^[A-Za-z][A-Za-z0-9]*$/.test(identifier)) {
  console.error(`Invalid C++ fixture identifier: ${identifier}`);
  process.exit(2);
}
const guardName = identifier
  .replace(/([a-z0-9])([A-Z])/g, "$1_$2")
  .toUpperCase();

const sourcePath = resolve(sourceArgument);
const outputPath = resolve(outputArgument);
const source = readFileSync(sourcePath);
const sha256 = createHash("sha256").update(source).digest("hex");

let fnv1a64 = 0xcbf29ce484222325n;
for (const byte of source) {
  fnv1a64 ^= BigInt(byte);
  fnv1a64 = BigInt.asUintN(64, fnv1a64 * 0x100000001b3n);
}
const fnvHex = fnv1a64.toString(16).padStart(16, "0");

const rows = [];
for (let offset = 0; offset < source.length; offset += 16) {
  const row = [...source.subarray(offset, offset + 16)]
    .map((byte) => `0x${byte.toString(16).padStart(2, "0")}`)
    .join(", ");
  rows.push(`    ${row},`);
}

const header = `#ifndef NMARKDOWN_FIREBIRD_${guardName}_FIXTURE_H\n` +
  `#define NMARKDOWN_FIREBIRD_${guardName}_FIXTURE_H\n\n` +
  `#include <cstddef>\n` +
  `#include <cstdint>\n\n` +
  `static const std::uint8_t k${identifier}Fixture[] = {\n` +
  `${rows.join("\n")}\n` +
  `};\n` +
  `static constexpr std::size_t k${identifier}FixtureSize = ` +
  `sizeof(k${identifier}Fixture);\n` +
  `static constexpr std::uint64_t k${identifier}FixtureFnv1a64 = ` +
  `UINT64_C(0x${fnvHex});\n` +
  `static constexpr char k${identifier}FixtureSha256[] = "${sha256}";\n\n` +
  `#endif\n`;

mkdirSync(dirname(outputPath), { recursive: true });
writeFileSync(outputPath, header);
console.log(
  `Embedded ${sourcePath}: ${source.length} bytes, sha256=${sha256}, ` +
  `fnv1a64=${fnvHex}`,
);
