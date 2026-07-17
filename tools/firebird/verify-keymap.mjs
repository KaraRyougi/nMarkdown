#!/usr/bin/env node

import {
  existsSync,
  mkdirSync,
  readFileSync,
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
  "build/ndless-firebird-keymap/nmarkdown-firebird-keymap.tns",
);
if (!existsSync(program)) {
  console.error(`Firebird keymap build is missing: ${program}`);
  process.exit(2);
}

const outputRoot = resolve(projectRoot, "build/firebird-keymap");
mkdirSync(outputRoot, { recursive: true });

// This is PocketJS's established TI-OS activation tape. The fixture-specific
// actions begin only after the three trailing waits have released launch keys.
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

const scenarios = [
  {
    name: "tab-page-down",
    actions: ["tab", "wait"],
    expectedInput: "KEYMAP_TAB_INPUT",
  },
  {
    name: "down-baseline",
    actions: ["tab", "wait", "down", "wait"],
    expectedInput: "KEYMAP_DOWN_INPUT",
  },
  {
    name: "enter-click-n2-liveness",
    actions: [
      "tab",
      "wait",
      "enter",
      "wait",
      "touch-center",
      "wait",
      "touch-release",
      "wait",
      "n2",
      "wait",
    ],
    expectedInput: "KEYMAP_N2_INPUT",
  },
  // PocketJS currently exposes physical n2..n9 but not n1. Host-side fake
  // Ndless matrix coverage pins Number1; this native run exercises every
  // navigation digit that the emulator harness can inject.
  {
    name: "search-navigation-digits-are-text",
    actions: [
      "tab",
      "wait",
      "doc",
      "wait",
      "hold:n2:80",
      "wait",
      "hold:n4:80",
      "wait",
      "hold:n6:80",
      "wait",
      "hold:n7:80",
      "wait",
      "hold:n8:80",
      "wait",
      "esc",
      "wait",
      "hold:n2:80",
      "wait",
    ],
    expectedInput: "KEYMAP_N2_INPUT",
  },
];

function requireOrdered(trace, markers, label) {
  let offset = 0;
  for (const marker of markers) {
    const index = trace.indexOf(marker, offset);
    if (index < 0) {
      throw new Error(`${label}: missing ordered marker: ${marker}`);
    }
    offset = index + marker.length;
  }
}

function scrollTransitions(trace) {
  return [...trace.matchAll(
    /NMARKDOWN_IT\/1 SCROLL_POSITION event=([a-z-]+) before=(-?\d+) after=(-?\d+)/g,
  )].map((match) => ({
    event: match[1],
    before: Number(match[2]),
    after: Number(match[3]),
  }));
}

function runScenario(scenario) {
  const outdir = resolve(outputRoot, scenario.name);
  mkdirSync(outdir, { recursive: true });
  const args = [
    resolve(pocketRoot, "scripts/nspire-firebird.ts"),
    "test",
    `--program=${program}`,
    "--remote=/ndless/startup/pocketjs-keymap.tns",
    `--outdir=${outdir}`,
    "--timeout-ms=180000",
    ...activation.map((key) => `--key=${key}`),
    ...scenario.actions.map((key) => `--key=${key}`),
    // Stable nMarkdown chrome/paper pixels used by the existing Scroll fixture.
    "--expect=0,0,0x3d4d",
    "--expect=0,2,0x1969",
    "--expect=0,18,0xffff",
    "--expect=319,20,0xffff",
    "--expect=0,238,0xffff",
    "--expect=319,239,0xffff",
  ];

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
    throw new Error(`${scenario.name}: PocketJS did not create its report/capture.`);
  }
  const report = JSON.parse(readFileSync(reportPath, "utf8"));
  if (report.status !== "pass" || report.mismatchedPixels !== 0) {
    throw new Error(`${scenario.name}: Firebird report did not pass cleanly.`);
  }

  requireOrdered(trace, [
    "NMARKDOWN_IT/1 ENTER_MAIN",
    "NMARKDOWN_IT/1 FIXTURE_READY",
    "NMARKDOWN_IT/1 SCROLL_SWIPE_READY",
    "NMARKDOWN_IT/1 DOCUMENT_READ",
    "NMARKDOWN_IT/1 DOCUMENT_PARSED",
    "NMARKDOWN_IT/1 DOCUMENT_READY",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    `NMARKDOWN_IT/1 ${scenario.expectedInput}`,
  ], scenario.name);

  return {
    ...scenario,
    outdir,
    trace,
    transitions: scrollTransitions(trace),
    frame: readFileSync(framePath),
  };
}

try {
  const [tab, down, liveness, searchDigits] = scenarios.map(runScenario);

  if (tab.transitions.length !== 1 ||
      tab.transitions[0].event !== "page-down" ||
      tab.transitions[0].before !== 0 ||
      tab.transitions[0].after <= 0) {
    throw new Error(
      `Tab did not produce exactly one PageDown transition: ` +
      JSON.stringify(tab.transitions),
    );
  }

  if (down.transitions.length !== 2 ||
      down.transitions[0].event !== "page-down" ||
      down.transitions[1].event !== "line-down" ||
      down.transitions[1].before !== down.transitions[0].after ||
      down.transitions[1].after <= down.transitions[1].before) {
    throw new Error(
      `Scroll-mode Down has unexpected transitions: ` +
      JSON.stringify(down.transitions),
    );
  }

  requireOrdered(liveness.trace, [
    "NMARKDOWN_IT/1 KEYMAP_TAB_INPUT",
    "NMARKDOWN_IT/1 SCROLL_POSITION event=page-down",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 KEYMAP_ENTER_INPUT",
    "NMARKDOWN_IT/1 KEYMAP_CLICK_INPUT",
    "NMARKDOWN_IT/1 KEYMAP_N2_INPUT",
    "NMARKDOWN_IT/1 SCROLL_POSITION event=line-down",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
  ], liveness.name);

  if (liveness.transitions.length !== 2 ||
      liveness.transitions[0].event !== "page-down" ||
      liveness.transitions[1].event !== "line-down") {
    throw new Error(
      `Enter/click changed scroll or n2 failed: ` +
      JSON.stringify(liveness.transitions),
    );
  }

  const baselineLine = down.transitions[1];
  const number2Line = liveness.transitions[1];
  if (baselineLine.before !== number2Line.before ||
      baselineLine.after !== number2Line.after) {
    throw new Error(
      `n2 did not match Down: down=${JSON.stringify(baselineLine)}, ` +
      `n2=${JSON.stringify(number2Line)}`,
    );
  }
  if (!down.frame.equals(liveness.frame)) {
    throw new Error(
      "The n2 final framebuffer differs from the Down baseline; " +
      "Enter/click may not be inert or n2 may not match Down.",
    );
  }
  if (tab.frame.equals(liveness.frame)) {
    throw new Error("The post-n2 framebuffer did not move beyond the Tab frame.");
  }

  requireOrdered(searchDigits.trace, [
    "NMARKDOWN_IT/1 KEYMAP_TAB_INPUT",
    "NMARKDOWN_IT/1 SCROLL_POSITION event=page-down",
    "NMARKDOWN_IT/1 KEYMAP_N2_INPUT",
    "NMARKDOWN_IT/1 SEARCH_QUERY value=2",
    "NMARKDOWN_IT/1 SEARCH_QUERY value=24",
    "NMARKDOWN_IT/1 SEARCH_QUERY value=246",
    "NMARKDOWN_IT/1 SEARCH_QUERY value=2467",
    "NMARKDOWN_IT/1 SEARCH_QUERY value=24678",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 KEYMAP_N2_INPUT",
    "NMARKDOWN_IT/1 SCROLL_POSITION event=line-down",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
  ], searchDigits.name);
  if (searchDigits.transitions.length !== 2 ||
      searchDigits.transitions[0].event !== "page-down" ||
      searchDigits.transitions[1].event !== "line-down" ||
      searchDigits.transitions[1].before !== searchDigits.transitions[0].after ||
      searchDigits.transitions[1].after <= searchDigits.transitions[1].before) {
    throw new Error(
      `Search digits navigated or normal input did not resume after Esc: ` +
      JSON.stringify(searchDigits.transitions),
    );
  }
  if (!down.frame.equals(searchDigits.frame)) {
    throw new Error(
      "Search entry, Esc, and n2 did not return to the Down baseline frame.",
    );
  }

  console.log("nMarkdown Firebird keymap regression: PASS");
  console.log(`Tab PageDown screenshot: ${resolve(tab.outdir, "frame.ppm")}`);
  console.log(`Down baseline screenshot: ${resolve(down.outdir, "frame.ppm")}`);
  console.log(`Enter/click + n2 screenshot: ${resolve(liveness.outdir, "frame.ppm")}`);
  console.log(
    `Search digits + Esc/n2 screenshot: ${resolve(searchDigits.outdir, "frame.ppm")}`,
  );
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exit(1);
}
