#!/usr/bin/env node

import {
  existsSync,
  mkdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { spawnSync } from "node:child_process";
import { homedir } from "node:os";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const pocketRoot = [
  process.env.POCKETJS_NSPIRE,
  resolve(homedir(), "Documents/PocketJS-NSpire"),
  resolve(projectRoot, "../PocketJS-NSpire"),
].filter(Boolean).find((candidate) =>
  existsSync(resolve(candidate, "scripts/nspire-firebird.ts")),
);

if (!pocketRoot) {
  console.error(
    "PocketJS-NSpire Firebird harness not found; set POCKETJS_NSPIRE.",
  );
  process.exit(2);
}

const program = resolve(
  projectRoot,
  "build/ndless-firebird-imath/nmarkdown-firebird-imath.tns",
);
if (!existsSync(program)) {
  console.error(`Firebird bold-italic i/j build is missing: ${program}`);
  process.exit(2);
}

const outdir = resolve(projectRoot, "build/firebird-imath");
rmSync(outdir, { recursive: true, force: true });
mkdirSync(outdir, { recursive: true });

// PocketJS's established TI-OS activation tape. The trailing waits guarantee
// that the final framebuffer is the reader, not a launch-key transition.
const activation = [
  "n2",
  "down",
  "enter",
  "enter",
  "esc",
  "down",
  "enter",
  "wait",
  "enter",
  "wait",
  "wait",
  "wait",
];

const args = [
  resolve(pocketRoot, "scripts/nspire-firebird.ts"),
  "test",
  `--program=${program}`,
  "--remote=/ndless/startup/pocketjs-integration.tns",
  `--outdir=${outdir}`,
  "--timeout-ms=180000",
  ...activation.map((key) => `--key=${key}`),
  // The zero-width progress track, reader chrome, and white paper establish
  // that the captured native frame is the start of this nMarkdown document.
  "--expect=0,0,0xbe39",
  "--expect=319,1,0xbe39",
  "--expect=0,2,0x1969",
  "--expect=0,18,0xffff",
  "--expect=319,20,0xffff",
  "--expect=0,238,0xffff",
  "--expect=319,239,0xffff",
];
if (process.env.FIREBIRD_CONFIG) {
  args.push(`--config=${resolve(process.env.FIREBIRD_CONFIG)}`);
}

const result = spawnSync(process.env.BUN ?? "bun", args, {
  cwd: pocketRoot,
  encoding: "utf8",
  maxBuffer: 8 * 1024 * 1024,
  env: process.env,
});
const trace = `${result.stdout ?? ""}\n${result.stderr ?? ""}`;
writeFileSync(resolve(outdir, "serial.log"), trace);
process.stdout.write(result.stdout ?? "");
process.stderr.write(result.stderr ?? "");
if (result.error) {
  console.error(result.error.message);
  process.exit(2);
}
if (result.status !== 0) process.exit(result.status ?? 2);

const reportPath = resolve(outdir, "result.json");
const framePath = resolve(outdir, "frame.ppm");
if (!existsSync(reportPath) || !existsSync(framePath)) {
  console.error("PocketJS did not create the bold-italic i/j report and frame.");
  process.exit(1);
}
const report = JSON.parse(readFileSync(reportPath, "utf8"));
if (report.status !== "pass" || report.mismatchedPixels !== 0 ||
    report.stableMatches < report.requiredStableMatches) {
  console.error("Bold-italic i/j Firebird report did not pass cleanly.");
  process.exit(1);
}

function requireOrdered(markers) {
  let offset = 0;
  for (const marker of markers) {
    const index = trace.indexOf(marker, offset);
    if (index < 0) {
      throw new Error(`Missing ordered serial marker: ${marker}`);
    }
    offset = index + marker.length;
  }
}

function readPpm(path) {
  const bytes = readFileSync(path);
  let offset = 0;
  const token = () => {
    while (offset < bytes.length) {
      if (bytes[offset] === 0x23) {
        while (offset < bytes.length && bytes[offset] !== 0x0a) ++offset;
      } else if (bytes[offset] <= 0x20) {
        ++offset;
      } else {
        break;
      }
    }
    const start = offset;
    while (offset < bytes.length && bytes[offset] > 0x20) ++offset;
    return bytes.subarray(start, offset).toString("ascii");
  };

  const magic = token();
  const width = Number(token());
  const height = Number(token());
  const maximum = Number(token());
  if (magic !== "P6" || width !== 320 || height !== 240 || maximum !== 255) {
    throw new Error(
      `${path}: expected native 320x240 P6, got ${magic} ${width}x${height}`,
    );
  }
  if (bytes[offset] === 0x0d && bytes[offset + 1] === 0x0a) offset += 2;
  else if (bytes[offset] <= 0x20) ++offset;
  const pixels = bytes.subarray(offset);
  if (pixels.length !== width * height * 3) {
    throw new Error(`${path}: invalid PPM pixel payload.`);
  }
  return { width, height, pixels };
}

function inkCount(frame, rectangle) {
  let count = 0;
  for (let y = rectangle.y; y < rectangle.y + rectangle.height; ++y) {
    for (let x = rectangle.x; x < rectangle.x + rectangle.width; ++x) {
      const offset = (y * frame.width + x) * 3;
      const red = frame.pixels[offset];
      const green = frame.pixels[offset + 1];
      const blue = frame.pixels[offset + 2];
      // Math foreground is #293441 with RGB565 anti-aliasing. A luminance
      // cutoff keeps the solid and useful edge coverage while excluding the
      // white paper; every rectangle below is isolated from its prose label.
      if (299 * red + 587 * green + 114 * blue < 192000) ++count;
    }
  }
  return count;
}

function inspectGlyphEvidence(frame) {
  const ordinaryI = inkCount(frame, { x: 193, y: 64, width: 6, height: 15 });
  const ordinaryJ = inkCount(frame, { x: 227, y: 64, width: 9, height: 16 });
  const boldItalicI = inkCount(
    frame,
    { x: 174, y: 92, width: 9, height: 17 },
  );
  const boldItalicJ = inkCount(
    frame,
    { x: 209, y: 92, width: 10, height: 18 },
  );
  const dotted = {
    ordinaryI: inkCount(frame, { x: 193, y: 64, width: 6, height: 4 }),
    ordinaryJ: inkCount(frame, { x: 229, y: 64, width: 7, height: 4 }),
    boldItalicI: inkCount(frame, { x: 175, y: 92, width: 7, height: 6 }),
    boldItalicJ: inkCount(frame, { x: 213, y: 92, width: 6, height: 6 }),
  };
  const accentRegions = [
    { name: "hat-imath", x: 119, y: 150, width: 11, height: 22 },
    { name: "hat-jmath", x: 141, y: 150, width: 11, height: 22 },
    { name: "vec-imath", x: 162, y: 150, width: 12, height: 22 },
    { name: "vec-jmath", x: 184, y: 150, width: 12, height: 22 },
  ].map((region) => ({
    name: region.name,
    ink: inkCount(frame, region),
    accentAndDotInk: inkCount(frame, {
      x: region.x,
      y: region.y,
      width: region.width,
      height: 12,
    }),
    letterInk: inkCount(frame, {
      x: region.x,
      y: region.y + 11,
      width: region.width,
      height: region.height - 11,
    }),
  }));

  if (ordinaryI < 8 || ordinaryJ < 12) {
    throw new Error(
      `Ordinary italic comparison glyphs are incomplete: i=${ordinaryI}, ` +
      `j=${ordinaryJ}.`,
    );
  }
  if (boldItalicI <= ordinaryI + 3 || boldItalicJ <= ordinaryJ + 3) {
    throw new Error(
      `Bold-italic commands are not visibly heavier than ordinary italic: ` +
      `i=${ordinaryI}->${boldItalicI}, j=${ordinaryJ}->${boldItalicJ}.`,
    );
  }
  for (const [name, count] of Object.entries(dotted)) {
    if (count < 1) {
      throw new Error(`${name} has no visible dot in its reviewed dot region.`);
    }
  }
  for (const region of accentRegions) {
    if (region.ink < 18 || region.accentAndDotInk < 5 ||
        region.letterInk < 10) {
      throw new Error(
        `${region.name} is incomplete: ${JSON.stringify(region)}.`,
      );
    }
  }
  return {
    ordinaryItalicInk: { i: ordinaryI, j: ordinaryJ },
    boldItalicInk: { i: boldItalicI, j: boldItalicJ },
    dotted,
    accented: accentRegions,
  };
}

function createPng(ppmPath, pngPath) {
  const conversion = spawnSync(
    "sips",
    ["-s", "format", "png", ppmPath, "--out", pngPath],
    { encoding: "utf8" },
  );
  if (conversion.error || conversion.status !== 0 || !existsSync(pngPath)) {
    throw new Error("Could not convert the native bold-italic i/j frame to PNG.");
  }
}

try {
  requireOrdered([
    "NMARKDOWN_IT/1 ENTER_MAIN",
    "NMARKDOWN_IT/1 FIXTURE_READY",
    "NMARKDOWN_IT/1 IMATH_FIXTURE_READY",
    "NMARKDOWN_IT/1 HARFBUZZ_READY",
    "NMARKDOWN_IT/1 DOCUMENT_READ",
    "NMARKDOWN_IT/1 DOCUMENT_PARSED",
    "NMARKDOWN_IT/1 DOCUMENT_READY",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
  ]);
  const frame = readPpm(framePath);
  const glyphEvidence = inspectGlyphEvidence(frame);
  const pngPath = resolve(outdir, "frame.png");
  createPng(framePath, pngPath);
  writeFileSync(
    resolve(outdir, "summary.json"),
    `${JSON.stringify({
      status: "pass",
      fixture: "bold-italic-imath-jmath",
      frameWidth: frame.width,
      frameHeight: frame.height,
      frameHashFnv1a64: report.frameHashFnv1a64,
      stableMatches: report.stableMatches,
      requiredStableMatches: report.requiredStableMatches,
      glyphEvidence,
      framePpm: framePath,
      framePng: pngPath,
      result: reportPath,
    }, null, 2)}\n`,
  );
  console.log(`nMarkdown bold-italic i/j: PASS (${pngPath})`);
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
