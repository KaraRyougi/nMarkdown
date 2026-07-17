#!/usr/bin/env node

import {spawnSync} from "node:child_process";
import {mkdtemp, readFile, rm, writeFile} from "node:fs/promises";
import {tmpdir} from "node:os";
import {join, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const fontPath = process.argv[2];
const headerPath = process.argv[3];
const fontOutputPath = process.argv[4];
if (!fontPath || !headerPath) {
    console.error(
        "Usage: node tools/firebird/build-font-fixture.mjs FONT.ttf OUTPUT.h [OUTPUT.ttf]",
    );
    process.exit(2);
}

const sample =
    "Monospace + CJK on CX II\n" +
    "Body fallback: 中文排版测试，日本語かなカナの表示。\n" +
    "Inline code: const width = 320;\n" +
    "Monospace metrics: MWi0\n" +
    "grid[02] = 320x240;\n" +
    "中文：标点换行，日本語かなカナ：句読点。\n" +
    "Sarasa Fixed SC was loaded directly as the monospace font.";
const projectRoot = fileURLToPath(new URL("../..", import.meta.url));
const printableAscii = Array.from({length: 95}, (_, index) =>
    String.fromCodePoint(0x20 + index)).join("");

async function buildSubset(path, text) {
    const sourcePath = resolve(path);
    const source = new Uint8Array(await readFile(sourcePath));
    const temporary = await mkdtemp(join(tmpdir(), "nmarkdown-font-fixture-"));
    const textPath = join(temporary, "characters.txt");
    const outputPath = join(temporary, "fixture.ttf");
    try {
        await writeFile(textPath, text, "utf8");
        const result = spawnSync("hb-subset", [
            sourcePath,
            `--text-file=${textPath}`,
            `--output-file=${outputPath}`,
            "--name-IDs=*",
            "--name-languages=*",
            "--name-legacy",
            "--layout-features=*",
            "--notdef-outline",
            "--glyph-names",
        ], {encoding: "utf8"});
        if (result.status !== 0) {
            throw new Error(result.stderr || "hb-subset failed");
        }
        return {source, font: new Uint8Array(await readFile(outputPath))};
    } finally {
        await rm(temporary, {recursive: true, force: true});
    }
}

function arrayDeclaration(name, font) {
    const rows = [];
    for (let offset = 0; offset < font.length; offset += 12) {
        rows.push("    " + [...font.subarray(offset, offset + 12)]
        .map((byte) => `0x${byte.toString(16).padStart(2, "0")}`)
        .join(", ") + ",");
    }
    return `static const std::uint8_t ${name}[] = {\n${rows.join("\n")}\n};\n` +
        `static constexpr std::size_t ${name}Size = sizeof(${name});\n`;
}

const cjk = await buildSubset(fontPath, sample);
const body = await buildSubset(
    resolve(projectRoot, "assets/fonts/DejaVuSans-CX.ttf"),
    printableAscii,
);
const italic = await buildSubset(
    resolve(projectRoot, "assets/fonts/DejaVuSans-Oblique-CX.ttf"),
    "italic",
);
const monospace = await buildSubset(
    resolve(projectRoot, "assets/fonts/DejaVuSansMono-CX.ttf"),
    printableAscii,
);

const header = `#ifndef NMARKDOWN_FIREBIRD_FONT_FIXTURE_H\n` +
    `#define NMARKDOWN_FIREBIRD_FONT_FIXTURE_H\n\n` +
    `#include <cstddef>\n#include <cstdint>\n\n` +
    `${arrayDeclaration("kFirebirdFontFixture", cjk.font)}\n` +
    `${arrayDeclaration("kFirebirdBodyFontFixture", body.font)}\n` +
    `${arrayDeclaration("kFirebirdItalicFontFixture", italic.font)}\n` +
    `${arrayDeclaration("kFirebirdMonoFontFixture", monospace.font)}\n` +
    `#endif\n`;
await writeFile(resolve(headerPath), header);
if (fontOutputPath) await writeFile(resolve(fontOutputPath), cjk.font);
console.log(
    `Firebird font fixtures: CJK ${cjk.source.length}->${cjk.font.length}, ` +
    `Body ${body.source.length}->${body.font.length}, ` +
    `Italic ${italic.source.length}->${italic.font.length}, ` +
    `Mono ${monospace.source.length}->${monospace.font.length}`,
);
