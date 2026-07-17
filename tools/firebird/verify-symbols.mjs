#!/usr/bin/env node

import { createHash } from "node:crypto";
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  statSync,
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
  "build/ndless-firebird-symbols/nmarkdown-firebird-symbols.tns",
);
if (!existsSync(program)) {
  console.error(`Firebird math-symbol build is missing: ${program}`);
  process.exit(2);
}

function projectSourceSnapshot() {
  const snapshot = new Map();
  const visit = (path) => {
    const status = statSync(path);
    if (status.isDirectory()) {
      for (const entry of readdirSync(path, { withFileTypes: true })) {
        if (entry.isDirectory() || entry.isFile()) {
          visit(resolve(path, entry.name));
        }
      }
      return;
    }
    snapshot.set(path, `${status.size}:${status.mtimeMs}`);
  };
  visit(resolve(projectRoot, "src"));
  visit(resolve(projectRoot, "include"));
  visit(resolve(projectRoot, "Makefile.ndless"));
  return snapshot;
}

function snapshotsEqual(left, right) {
  if (left.size !== right.size) return false;
  for (const [path, signature] of left) {
    if (right.get(path) !== signature) return false;
  }
  return true;
}

const startingSources = projectSourceSnapshot();
const programModified = statSync(program).mtimeMs;
for (const [path, signature] of startingSources) {
  const modified = Number(signature.slice(signature.indexOf(":") + 1));
  if (modified > programModified) {
    console.error(
      `Firebird math-symbol build is stale relative to ${path}; rebuild it.`,
    );
    process.exit(2);
  }
}

const sourcePath = resolve(projectRoot, "samples/math-symbol-gallery.md");
const source = readFileSync(sourcePath);
const sourceSha256 = createHash("sha256").update(source).digest("hex");
let sourceFnv1a64 = 0xcbf29ce484222325n;
for (const byte of source) {
  sourceFnv1a64 ^= BigInt(byte);
  sourceFnv1a64 = BigInt.asUintN(64, sourceFnv1a64 * 0x100000001b3n);
}
const sourceFnvHex = sourceFnv1a64.toString(16).padStart(16, "0");

const outputRoot = resolve(projectRoot, "build/firebird-symbols");
const artifactRoot = resolve(projectRoot, "artifacts");
mkdirSync(outputRoot, { recursive: true });
mkdirSync(artifactRoot, { recursive: true });

// PocketJS's established TI-OS activation tape. Scenario actions start only
// after the final waits have released every launch key.
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

const repeated = (key, count) =>
  Array.from({ length: count }, () => [key, "wait"]).flat();
// The gallery is deliberately small, but the extra clamped steps also prove
// that screen-step navigation remains live after reaching the document end.
const driveToEnd = repeated("tab", 16);

function requireOrdered(trace, markers, label) {
  let offset = 0;
  for (const marker of markers) {
    const index = trace.indexOf(marker, offset);
    if (index < 0) {
      throw new Error(`${label}: missing ordered serial marker: ${marker}`);
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
      `${path}: expected native 320x240 P6, got ` +
      `${magic} ${width}x${height} max=${maximum}`,
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

function pixel565(frame, x, y) {
  const offset = (y * frame.width + x) * 3;
  const red = frame.pixels[offset];
  const green = frame.pixels[offset + 1];
  const blue = frame.pixels[offset + 2];
  return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
}

function progressWidth(frame, label) {
  const progress = 0x3d4d;
  const track = 0xbe39;
  let width = 0;
  while (width < frame.width && pixel565(frame, width, 0) === progress) {
    ++width;
  }
  for (let y = 0; y < 2; ++y) {
    for (let x = 0; x < frame.width; ++x) {
      const expected = x < width ? progress : track;
      const actual = pixel565(frame, x, y);
      if (actual !== expected) {
        throw new Error(
          `${label}: progress row ${y} is not a single ${width}px fill at ` +
          `x=${x} (0x${actual.toString(16)} != 0x${expected.toString(16)}).`,
        );
      }
    }
  }
  return width;
}

function bodyEvidence(frame) {
  let ink = 0;
  let minimumX = frame.width;
  let minimumY = frame.height;
  let maximumX = -1;
  let maximumY = -1;
  for (let y = 18; y < 238; ++y) {
    for (let x = 1; x < 319; ++x) {
      const offset = (y * frame.width + x) * 3;
      const red = frame.pixels[offset];
      const green = frame.pixels[offset + 1];
      const blue = frame.pixels[offset + 2];
      // Body text, headings, and anti-aliased Latin Modern glyphs are all
      // substantially darker than the white paper. This intentionally counts
      // every visible glyph rather than relying on fragile command-specific
      // pixels.
      if (299 * red + 587 * green + 114 * blue >= 220000) continue;
      ++ink;
      minimumX = Math.min(minimumX, x);
      minimumY = Math.min(minimumY, y);
      maximumX = Math.max(maximumX, x);
      maximumY = Math.max(maximumY, y);
    }
  }
  return {
    ink,
    bounds: ink === 0
      ? null
      : {
        x: minimumX,
        y: minimumY,
        width: maximumX - minimumX + 1,
        height: maximumY - minimumY + 1,
      },
  };
}

function progressRecords(trace) {
  return [...trace.matchAll(
    /READING_PROGRESS mode=(vertical|horizontal) scroll=(-?\d+) max=(-?\d+) page=(\d+) total=(\d+) width=(\d+)/g,
  )].map((match) => ({
    mode: match[1],
    scroll: Number(match[2]),
    maximumScroll: Number(match[3]),
    page: Number(match[4]),
    totalPages: Number(match[5]),
    width: Number(match[6]),
  }));
}

function createPng(ppmPath, pngPath) {
  const conversion = spawnSync(
    "sips",
    ["-s", "format", "png", ppmPath, "--out", pngPath],
    { encoding: "utf8" },
  );
  if (conversion.error || conversion.status !== 0 || !existsSync(pngPath)) {
    console.warn(
      `PNG conversion unavailable for ${ppmPath}; keeping authoritative PPM.`,
    );
    return null;
  }
  return pngPath;
}

function validNativeReport(report, framePath) {
  return Number.isInteger(report.schemaVersion) && report.schemaVersion >= 11 &&
    report.status === "pass" && report.transferProgress === 100 &&
    report.expectedTransferFiles === 1 && report.completedTransferFiles === 1 &&
    report.mismatchedPixels === 0 &&
    report.stableMatches >= report.requiredStableMatches &&
    Array.isArray(report.expectations) && report.expectations.length >= 5 &&
    report.expectations.every((expectation) => expectation.matches === true) &&
    existsSync(framePath);
}

function runScenario(name, actions, expectedScroll = null) {
  const outdir = resolve(outputRoot, name);
  rmSync(outdir, { recursive: true, force: true });
  mkdirSync(outdir, { recursive: true });
  const args = [
    resolve(pocketRoot, "scripts/nspire-firebird.ts"),
    "test",
    `--program=${program}`,
    "--remote=/ndless/startup/pocketjs-integration.tns",
    `--outdir=${outdir}`,
    "--timeout-ms=180000",
    ...activation.map((key) => `--key=${key}`),
    ...actions.map((key) => `--key=${key}`),
    // Stable chrome and the two-pixel bottom inset distinguish the reader from
    // TI-OS while leaving all symbol pixels free for visual review.
    "--expect=0,2,0x1969",
    "--expect=0,18,0xffff",
    "--expect=319,20,0xffff",
    "--expect=0,238,0xffff",
    "--expect=319,239,0xffff",
  ];
  if (process.env.FIREBIRD_CONFIG) {
    args.push(`--config=${resolve(process.env.FIREBIRD_CONFIG)}`);
  }

  console.log(`Firebird symbol gallery: ${name}`);
  const result = spawnSync(process.env.BUN ?? "bun", args, {
    cwd: pocketRoot,
    encoding: "utf8",
    maxBuffer: 8 * 1024 * 1024,
    env: process.env,
  });
  const trace = `${result.stdout ?? ""}\n${result.stderr ?? ""}`;
  writeFileSync(resolve(outdir, "serial.log"), trace);
  if (result.error) {
    process.stdout.write(result.stdout ?? "");
    process.stderr.write(result.stderr ?? "");
    throw result.error;
  }

  const reportPath = resolve(outdir, "result.json");
  const framePath = resolve(outdir, "frame.ppm");
  if (!existsSync(reportPath)) {
    throw new Error(`${name}: PocketJS did not create result.json.`);
  }
  const report = JSON.parse(readFileSync(reportPath, "utf8"));
  const reportValid = validNativeReport(report, framePath);
  if (result.status !== 0) {
    const knownSchemaMismatch =
      /unsupported[^\n]*schema|schema[^\n]*unsupported/i.test(trace) &&
      report.schemaVersion >= 13 && reportValid;
    if (!knownSchemaMismatch) {
      process.stdout.write(result.stdout ?? "");
      process.stderr.write(result.stderr ?? "");
      throw new Error(
        `${name}: PocketJS exited with ${result.status}; inspect ${outdir}.`,
      );
    }
    console.log(
      `${name}: accepted independently validated schema-${report.schemaVersion} ` +
      "native pass after the external wrapper rejected its newer schema.",
    );
  } else if (!reportValid) {
    throw new Error(`${name}: native Firebird report did not pass cleanly.`);
  }

  requireOrdered(trace, [
    "NMARKDOWN_IT/1 ENTER_MAIN",
    "NMARKDOWN_IT/1 FIXTURE_READY",
    `NMARKDOWN_IT/1 SYMBOL_GALLERY_EXACT bytes=${source.length} ` +
      `fnv1a64=${sourceFnvHex} sha256=${sourceSha256}`,
    "NMARKDOWN_IT/1 SYMBOL_GALLERY_READY",
    "NMARKDOWN_IT/1 HARFBUZZ_READY",
    "NMARKDOWN_IT/1 DOCUMENT_READ",
    "NMARKDOWN_IT/1 DOCUMENT_PARSED",
    "NMARKDOWN_IT/1 DOCUMENT_READY",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
  ], name);

  const frame = readPpm(framePath);
  const width = progressWidth(frame, name);
  const records = progressRecords(trace);
  if (records.length === 0) {
    throw new Error(`${name}: no reading-progress record was emitted.`);
  }
  const finalProgress = records.at(-1);
  if (finalProgress.mode !== "horizontal" || finalProgress.width !== width) {
    throw new Error(
      `${name}: trace/frame progress disagreement: ` +
      `${JSON.stringify(finalProgress)}, framebuffer=${width}.`,
    );
  }
  const expectedWidth = finalProgress.maximumScroll <= 0
    ? 0
    : Math.floor(320 * Math.max(0, Math.min(
      finalProgress.maximumScroll,
      finalProgress.scroll,
    )) / finalProgress.maximumScroll);
  if (width !== expectedWidth) {
    throw new Error(
      `${name}: expected ${expectedWidth}px continuous progress at ` +
      `${finalProgress.scroll}/${finalProgress.maximumScroll}, got ${width}px.`,
    );
  }
  if (expectedScroll !== null && finalProgress.scroll !== expectedScroll) {
    throw new Error(
      `${name}: expected scroll ${expectedScroll}, got ${finalProgress.scroll}.`,
    );
  }

  const evidence = bodyEvidence(frame);
  if (evidence.ink < 60 || evidence.bounds === null ||
      evidence.bounds.height < 10) {
    throw new Error(
      `${name}: captured page lacks readable body glyph evidence: ` +
      `${JSON.stringify(evidence)}.`,
    );
  }

  const pngPath = createPng(framePath, resolve(outdir, "frame.png"));
  return {
    name,
    finalProgress,
    progressRecords: records,
    progressWidth: width,
    bodyEvidence: evidence,
    frameHashFnv1a64: report.frameHashFnv1a64,
    schemaVersion: report.schemaVersion,
    stableMatches: report.stableMatches,
    requiredStableMatches: report.requiredStableMatches,
    framePpm: framePath,
    framePng: pngPath,
    result: reportPath,
  };
}

try {
  // Discover every distinct context-preserving screen-step position and the
  // measured document end in one cold run. Independent captures then replay
  // the same number of forward steps from the start.
  const finalCapture = runScenario("discover-end", driveToEnd);
  if (finalCapture.finalProgress.scroll !==
        finalCapture.finalProgress.maximumScroll ||
      finalCapture.progressWidth !== 320) {
    throw new Error(
      `Discovery did not reach the document end: ` +
      `${JSON.stringify(finalCapture.finalProgress)}.`,
    );
  }
  const discoveredPositions = [];
  for (const record of finalCapture.progressRecords) {
    if (discoveredPositions.at(-1) !== record.scroll) {
      discoveredPositions.push(record.scroll);
    }
  }
  if (discoveredPositions[0] !== 0 ||
      discoveredPositions.at(-1) !== finalCapture.finalProgress.maximumScroll ||
      discoveredPositions.length < 2 || discoveredPositions.length > 16) {
    throw new Error(
      `Unexpected symbol-gallery screen positions: ${JSON.stringify(discoveredPositions)}.`,
    );
  }

  const captures = [];
  for (let screen = 0; screen < discoveredPositions.length; ++screen) {
    const capture = screen === discoveredPositions.length - 1
      ? finalCapture
      : runScenario(
        `screen-${String(screen + 1).padStart(2, "0")}`,
        repeated("tab", screen),
        discoveredPositions[screen],
      );
    const suffix = String(screen + 1).padStart(2, "0");
    const artifactPpm = resolve(
      artifactRoot,
      `math-symbol-gallery-firebird-screen-${suffix}.ppm`,
    );
    copyFileSync(capture.framePpm, artifactPpm);
    let artifactPng = null;
    if (capture.framePng !== null) {
      artifactPng = resolve(
        artifactRoot,
        `math-symbol-gallery-firebird-screen-${suffix}.png`,
      );
      copyFileSync(capture.framePng, artifactPng);
    }
    captures.push({ ...capture, screen: screen + 1, artifactPpm, artifactPng });
    console.log(
      `symbol screen ${screen + 1}/${discoveredPositions.length}: ` +
      `${capture.frameHashFnv1a64}; ${artifactPng ?? artifactPpm}`,
    );
  }

  const hashes = new Set(captures.map((capture) => capture.frameHashFnv1a64));
  if (hashes.size !== captures.length) {
    throw new Error("Two symbol-gallery pages produced the same native frame.");
  }
  if (!snapshotsEqual(startingSources, projectSourceSnapshot())) {
    throw new Error(
      "Project sources changed during the native gallery run; rebuild and " +
      "repeat before accepting these captures.",
    );
  }
  const summaryPath = resolve(outputRoot, "summary.json");
  writeFileSync(
    summaryPath,
    `${JSON.stringify({
      status: "pass",
      fixture: "math-symbol-gallery",
      source: sourcePath,
      sourceBytes: source.length,
      sourceSha256,
      sourceFnv1a64: sourceFnvHex,
      totalScreens: discoveredPositions.length,
      discoveredPositions,
      captures,
    }, null, 2)}\n`,
  );
  console.log(
    `nMarkdown native math-symbol gallery: PASS ` +
    `(${discoveredPositions.length} screens; ${summaryPath})`,
  );
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
