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

const programs = {
  vertical: resolve(
    projectRoot,
    "build/ndless-firebird-scroll-swipe/nmarkdown-firebird-scroll-swipe.tns",
  ),
  horizontal: resolve(
    projectRoot,
    "build/ndless-firebird-page/nmarkdown-firebird-page.tns",
  ),
};
for (const [mode, program] of Object.entries(programs)) {
  if (!existsSync(program)) {
    console.error(`Firebird ${mode} progress build is missing: ${program}`);
    process.exit(2);
  }
}

const outputRoot = resolve(projectRoot, "build/firebird-progress");
mkdirSync(outputRoot, { recursive: true });

// This is PocketJS's established TI-OS activation tape. Scenario actions begin
// only after the trailing waits have released every launch key.
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

const swipeDown = [
  "touch-center",
  "wait",
  "touch-down",
  "wait",
  "touch-release",
  "wait",
];

const dragLeft = [
  "touch-center",
  "wait",
  "touch-left",
  "wait",
  "touch-release",
  "wait",
];

const scenarios = [
  {
    name: "vertical-start",
    mode: "vertical",
    actions: [],
    expectedWidth: 0,
  },
  {
    name: "vertical-after-down",
    mode: "vertical",
    actions: ["down", "wait"],
    widthRule: "partial",
  },
  {
    name: "vertical-end",
    mode: "vertical",
    // Start with a physical Down so the scroll probe records a known 0 -> 18
    // transition, then use screen-step keys until the lower clamp is reached.
    // Five Tabs reach this fixture's measured lower clamp; the sixth proves a
    // further Page Down cannot extend progress beyond 320 pixels.
    actions: ["down", "wait", ...repeated("tab", 6)],
    expectedWidth: 320,
  },
  {
    name: "horizontal-start",
    mode: "horizontal",
    actions: [],
    expectedWidth: 0,
  },
  {
    name: "horizontal-continuous",
    mode: "horizontal",
    // Horizontal motion is continuous in Horizontal Scroll. Natural uses a
    // leftward drag to advance from page 1; it must move away from zero
    // without producing a discrete PAGE_MOVE transition.
    actions: [...dragLeft],
    continuousHorizontal: true,
  },
  {
    name: "horizontal-screen-step",
    mode: "horizontal",
    // Natural discrete swiping follows reading order, so one downward swipe
    // advances exactly one context-preserving screen step.
    actions: [...swipeDown],
    widthRule: "partial",
    screenStepMarker: /PAGE_MOVE[^\n]*scroll=[1-9]\d*/,
  },
  {
    name: "horizontal-end",
    mode: "horizontal",
    actions: [...swipeDown, ...repeated("tab", 6)],
    expectedWidth: 320,
  },
];

// Keep the default command as the complete suite, but allow a comma-separated
// subset while iterating on one tape. This avoids rebooting Firebird for every
// already-passing scenario during targeted gesture work.
const requestedScenarioNames = (process.env.FIREBIRD_PROGRESS_SCENARIOS ?? "")
  .split(",")
  .map((name) => name.trim())
  .filter(Boolean);
const requestedScenarioSet = new Set(requestedScenarioNames);
const selectedScenarios = requestedScenarioNames.length === 0
  ? scenarios
  : scenarios.filter((scenario) => requestedScenarioSet.has(scenario.name));
const knownScenarioNames = new Set(scenarios.map((scenario) => scenario.name));
const unknownScenarioNames = requestedScenarioNames.filter(
  (name) => !knownScenarioNames.has(name),
);
if (unknownScenarioNames.length !== 0) {
  console.error(
    `Unknown Firebird progress scenario(s): ${unknownScenarioNames.join(", ")}`,
  );
  process.exit(2);
}

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
      `${path}: expected a native 320x240 P6 capture, got ` +
      `${magic} ${width}x${height} max=${maximum}`,
    );
  }
  // A binary PPM has exactly one whitespace separator after maxval. Account
  // for CRLF without skipping a legitimate low-valued first pixel byte.
  if (bytes[offset] === 0x0d && bytes[offset + 1] === 0x0a) offset += 2;
  else if (bytes[offset] <= 0x20) ++offset;
  const pixels = bytes.subarray(offset);
  if (pixels.length !== width * height * 3) {
    throw new Error(`${path}: truncated or overlong PPM pixel payload.`);
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

const progressColor = 0x3d4d;
const trackColor = 0xbe39;

function progressWidth(frame, label) {
  let width = 0;
  while (width < frame.width && pixel565(frame, width, 0) === progressColor) {
    ++width;
  }
  for (let y = 0; y < 2; ++y) {
    for (let x = 0; x < frame.width; ++x) {
      const expected = x < width ? progressColor : trackColor;
      const actual = pixel565(frame, x, y);
      if (actual !== expected) {
        throw new Error(
          `${label}: progress row ${y} is not a single ${width}px fill; ` +
          `x=${x}, expected=0x${expected.toString(16)}, ` +
          `actual=0x${actual.toString(16)}`,
        );
      }
    }
  }
  return width;
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

function createPng(framePath, pngPath) {
  const result = spawnSync(
    "sips",
    ["-s", "format", "png", framePath, "--out", pngPath],
    { encoding: "utf8" },
  );
  if (result.error || result.status !== 0 || !existsSync(pngPath)) {
    console.warn(
      `PNG conversion unavailable for ${framePath}; the native PPM remains ` +
      `the authoritative screenshot.`,
    );
    return null;
  }
  return pngPath;
}

function runScenario(scenario) {
  const outdir = resolve(outputRoot, scenario.name);
  rmSync(outdir, { recursive: true, force: true });
  mkdirSync(outdir, { recursive: true });
  const args = [
    resolve(pocketRoot, "scripts/nspire-firebird.ts"),
    "test",
    `--program=${programs[scenario.mode]}`,
    // Keep the known startup name so TI-OS ordering and activation are stable.
    "--remote=/ndless/startup/pocketjs-integration.tns",
    `--outdir=${outdir}`,
    "--timeout-ms=180000",
    ...activation.map((key) => `--key=${key}`),
    ...scenario.actions.map((key) => `--key=${key}`),
    // Stable app chrome and paper pixels establish that the capture belongs to
    // nMarkdown rather than TI-OS or a transfer dialog.
    "--expect=0,2,0x1969",
    "--expect=0,18,0xffff",
    "--expect=319,20,0xffff",
    "--expect=0,238,0xffff",
    "--expect=319,239,0xffff",
  ];
  if (scenario.expectedWidth === 0) {
    args.push("--expect=0,0,0xbe39", "--expect=319,1,0xbe39");
  } else if (scenario.expectedWidth !== undefined) {
    args.push("--expect=0,0,0x3d4d");
    if (scenario.expectedWidth === 160) {
      args.push("--expect=159,1,0x3d4d", "--expect=160,0,0xbe39");
    } else if (scenario.expectedWidth === 320) {
      args.push("--expect=319,1,0x3d4d");
    } else {
      args.push("--expect=319,1,0xbe39");
    }
  } else if (scenario.widthRule === "partial") {
    args.push("--expect=0,0,0x3d4d", "--expect=319,1,0xbe39");
  }

  console.log(`Firebird progress scenario: ${scenario.name}`);
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
  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(
      `${scenario.name}: PocketJS exited with ${result.status}; inspect ${outdir}`,
    );
  }

  const reportPath = resolve(outdir, "result.json");
  const framePath = resolve(outdir, "frame.ppm");
  if (!existsSync(reportPath) || !existsSync(framePath)) {
    throw new Error(`${scenario.name}: PocketJS did not create report/capture.`);
  }
  const report = JSON.parse(readFileSync(reportPath, "utf8"));
  if (report.status !== "pass" || report.mismatchedPixels !== 0 ||
      report.stableMatches < report.requiredStableMatches) {
    throw new Error(`${scenario.name}: Firebird report did not pass cleanly.`);
  }

  requireOrdered(trace, [
    "NMARKDOWN_IT/1 ENTER_MAIN",
    "NMARKDOWN_IT/1 FIXTURE_READY",
    ...(scenario.mode === "vertical"
      ? ["NMARKDOWN_IT/1 SCROLL_SWIPE_READY"]
      : []),
    "NMARKDOWN_IT/1 HARFBUZZ_READY",
    "NMARKDOWN_IT/1 DOCUMENT_READ",
    "NMARKDOWN_IT/1 DOCUMENT_PARSED",
    "NMARKDOWN_IT/1 DOCUMENT_READY",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
  ], scenario.name);

  if (scenario.name === "vertical-after-down" ||
      scenario.name === "vertical-end") {
    if (!trace.includes("NMARKDOWN_IT/1 SCROLL_SWIPE_BASE_DOWN_INPUT") ||
        !/SCROLL_POSITION event=line-down before=0 after=18/.test(trace)) {
      throw new Error(`${scenario.name}: physical Down did not move 0 -> 18.`);
    }
  }
  if (scenario.screenStepMarker && !scenario.screenStepMarker.test(trace)) {
    throw new Error(`${scenario.name}: expected screen-step trace was absent.`);
  }

  const frame = readPpm(framePath);
  const width = progressWidth(frame, scenario.name);
  const records = progressRecords(trace);
  if (records.length === 0) {
    throw new Error(`${scenario.name}: missing post-render progress trace.`);
  }
  const finalProgress = records[records.length - 1];
  if (finalProgress.mode !== scenario.mode || finalProgress.width !== width) {
    throw new Error(
      `${scenario.name}: trace/frame disagreement: ` +
      `${JSON.stringify(finalProgress)}, framebuffer=${width}px.`,
    );
  }
  const formulaWidth = finalProgress.maximumScroll <= 0
    ? 0
    : Math.floor(320 * Math.max(0, Math.min(
      finalProgress.maximumScroll,
      finalProgress.scroll,
    )) / finalProgress.maximumScroll);
  if (width !== formulaWidth) {
    throw new Error(
      `${scenario.name}: ${scenario.mode} formula gives ${formulaWidth}px, ` +
      `frame/trace give ${width}px (${JSON.stringify(finalProgress)}).`,
    );
  }
  if (scenario.continuousHorizontal &&
      (!(finalProgress.scroll > 0 && finalProgress.scroll < 220) ||
       trace.includes("NMARKDOWN_IT/1 PAGE_MOVE"))) {
    throw new Error(
      `${scenario.name}: expected a continuous sub-page horizontal move, got ` +
      `${JSON.stringify(finalProgress)}.`,
    );
  }
  if (scenario.expectedWidth !== undefined && width !== scenario.expectedWidth) {
    throw new Error(
      `${scenario.name}: progress width=${width}, expected=${scenario.expectedWidth}.`,
    );
  }
  if (scenario.widthRule === "partial" && !(width > 0 && width < 320)) {
    throw new Error(`${scenario.name}: expected partial progress, got ${width}px.`);
  }

  const pngPath = createPng(framePath, resolve(outdir, "frame.png"));
  console.log(`${scenario.name}: ${width}px progress; ${pngPath ?? framePath}`);
  return {
    name: scenario.name,
    mode: scenario.mode,
    progressWidth: width,
    finalProgress,
    scrollTransitions: [...trace.matchAll(
      /SCROLL_POSITION event=([a-z-]+) before=(-?\d+) after=(-?\d+)/g,
    )].map((match) => ({
      event: match[1],
      before: Number(match[2]),
      after: Number(match[3]),
    })),
    framePpm: framePath,
    framePng: pngPath,
    result: reportPath,
    frameHashFnv1a64: report.frameHashFnv1a64,
  };
}

try {
  const results = selectedScenarios.map(runScenario);
  writeFileSync(
    resolve(outputRoot, "result.json"),
    `${JSON.stringify({ status: "pass", scenarios: results }, null, 2)}\n`,
  );
  console.log("nMarkdown Firebird progress regression: PASS");
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
