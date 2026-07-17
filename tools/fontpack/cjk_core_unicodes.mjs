#!/usr/bin/env node

import {readFileSync, writeFileSync} from "node:fs";

const [, , cp936Path, cp932Path, outputPath] = process.argv;
if (!cp936Path || !cp932Path || !outputPath) {
    console.error("usage: cjk_core_unicodes.mjs CP936.TXT CP932.TXT OUTPUT.unicodes");
    process.exit(2);
}

function mappings(path) {
    const result = [];
    for (const line of readFileSync(path, "utf8").split(/\r?\n/)) {
        const match = /^0x([0-9A-F]+)\s+0x([0-9A-F]+)/i.exec(line);
        if (match) {
            result.push([Number.parseInt(match[1], 16),
                         Number.parseInt(match[2], 16)]);
        }
    }
    return result;
}

const codepoints = new Set();
for (const [encoded, codepoint] of mappings(cp936Path)) {
    const lead = encoded >>> 8;
    const trail = encoded & 0xff;
    if (lead >= 0xa1 && lead <= 0xf7 && trail >= 0xa1 && trail <= 0xfe) {
        codepoints.add(codepoint);
    }
}
for (const [encoded, codepoint] of mappings(cp932Path)) {
    const lead = encoded >>> 8;
    const trail = encoded & 0xff;
    const jisLead = (lead >= 0x81 && lead <= 0x9f) ||
                    (lead >= 0xe0 && lead <= 0xef);
    const jisTrail = (trail >= 0x40 && trail <= 0x7e) ||
                     (trail >= 0x80 && trail <= 0xfc);
    if (jisLead && jisTrail) codepoints.add(codepoint);
}

const extraRanges = [
    [0x2000, 0x206f], [0x20a0, 0x20cf], [0x2100, 0x214f],
    [0x2190, 0x21ff], [0x2500, 0x26ff], [0x2e80, 0x303f],
    [0x3040, 0x30ff], [0x31f0, 0x31ff], [0xfe10, 0xfe1f],
    [0xfe30, 0xfe4f], [0xff00, 0xffef], [0xfffd, 0xfffd],
];
for (const [first, last] of extraRanges) {
    for (let value = first; value <= last; ++value) codepoints.add(value);
}

const values = [...codepoints].sort((left, right) => left - right);
const ranges = [];
const hex = (value) => value.toString(16).toUpperCase().padStart(4, "0");
for (let index = 0; index < values.length;) {
    const first = values[index];
    let last = first;
    while (index + 1 < values.length && values[index + 1] === last + 1) {
        last = values[++index];
    }
    ranges.push(first === last ? `U+${hex(first)}`
                               : `U+${hex(first)}-${hex(last)}`);
    ++index;
}
const lines = [];
for (let index = 0; index < ranges.length; index += 8) {
    lines.push(ranges.slice(index, index + 8).join(","));
}
writeFileSync(outputPath, `${lines.join(",\n")}\n`);
console.log(`wrote ${values.length} codepoints in ${ranges.length} ranges`);
