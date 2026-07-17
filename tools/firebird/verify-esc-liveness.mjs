#!/usr/bin/env node

import { createHash } from "node:crypto";
import {
  existsSync,
  mkdirSync,
  readFileSync,
  writeFileSync,
} from "node:fs";
import { homedir } from "node:os";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const sourcePath = resolve(
  process.env.MARKDOWN_FORMULA_DOCUMENT ??
    "/Users/ryougi/Downloads/markdown-formula.md",
);
if (!existsSync(sourcePath)) {
  console.error(`Exact Markdown reproduction source is missing: ${sourcePath}`);
  process.exit(2);
}
const source = readFileSync(sourcePath);
const sourceSha256 = createHash("sha256").update(source).digest("hex");

const pocketCandidates = [
  process.env.POCKETJS_NSPIRE,
  resolve(homedir(), "Documents/PocketJS-NSpire"),
  resolve(projectRoot, "../PocketJS-NSpire"),
].filter(Boolean);
const pocketRoot = pocketCandidates.find((candidate) =>
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
  "build/ndless-firebird-esc-liveness/nmarkdown-firebird-esc-liveness.tns",
);
if (!existsSync(program)) {
  console.error(`Esc-liveness integration build is missing: ${program}`);
  process.exit(2);
}

const outdir = resolve(projectRoot, "build/firebird-esc-liveness");
mkdirSync(outdir, { recursive: true });
const args = [
  resolve(pocketRoot, "scripts/nspire-firebird.ts"),
  "test",
  `--program=${program}`,
  // Preserve the established startup filename so TI-OS sorting and the
  // activation tape select the same transferred entry as every other fixture.
  "--remote=/ndless/startup/pocketjs-integration.tns",
  `--outdir=${outdir}`,
  "--timeout-ms=240000",
  // Enter My Documents and launch the transferred Ndless program using the
  // same deterministic activation tape as the established fixtures.
  "--key=n2",
  "--key=down",
  "--key=enter",
  "--key=enter",
  "--key=esc",
  "--key=down",
  "--key=enter",
  "--key=wait",
  "--key=enter",
  "--key=wait",
  "--key=wait",
  "--key=wait",
  // This exact Chinese document first opens its automatic CJK-font hint.
  // Close that hint, wait through a real release interval, and prove Down
  // reaches the document before testing the independent contents menu path.
  "--key=esc",
  "--key=wait",
  "--key=down",
  "--key=wait",
  // Doc is the production section-list key. It opens the actual TOC because
  // the automatic CJK hint has already been dismissed.
  "--key=doc",
  "--key=wait",
  "--key=esc",
  "--key=wait",
  "--key=down",
  "--key=wait",
  "--key=ctrl-esc",
  "--key=wait",
  // Dismiss a possible TI-OS post-Ndless notice after the app exits.
  "--key=esc",
  "--key=wait",
  // These are the established TI-OS return-frame pixels used by the state
  // fixture. If Ctrl+Esc is not consumed, the still-open reader cannot match.
  "--expect=0,0,0x31a6",
  "--expect=319,20,0x31a6",
  "--expect=0,45,0x0000",
  "--expect=20,55,0x23d9",
  "--expect=300,100,0x0000",
  "--expect=319,239,0x0000",
];
if (process.env.FIREBIRD_CONFIG) {
  args.push(`--config=${resolve(process.env.FIREBIRD_CONFIG)}`);
}

const result = spawnSync(process.env.BUN ?? "bun", args, {
  cwd: pocketRoot,
  encoding: "utf8",
  maxBuffer: 16 * 1024 * 1024,
  env: process.env,
});
const serialTrace = `${result.stdout ?? ""}\n${result.stderr ?? ""}`;
process.stdout.write(result.stdout ?? "");
process.stderr.write(result.stderr ?? "");
writeFileSync(resolve(outdir, "serial.log"), serialTrace);

function markerPosition(marker) {
  return serialTrace.lastIndexOf(`NMARKDOWN_IT/1 ${marker}`);
}

function diagnoseLastStage() {
  const pollEnter = markerPosition("TRACE_POLL_ENTER");
  const pollReturn = markerPosition("TRACE_POLL_RETURN");
  if (pollEnter > pollReturn) return "blocked inside InputNdless::poll";

  const sleepEnter = markerPosition("TRACE_SLEEP_ENTER");
  const sleepReturn = markerPosition("TRACE_SLEEP_RETURN");
  if (sleepEnter > sleepReturn) return "blocked inside ClockNdless::sleep_ms";

  const renderStart = markerPosition("RENDER_START");
  const renderDone = markerPosition("RENDER_DONE");
  if (renderStart > renderDone) return "blocked while rendering the viewer";

  const presentEnter = markerPosition("TRACE_PRESENT_ENTER");
  const presentReturn = markerPosition("TRACE_PRESENT_RETURN");
  if (presentEnter > presentReturn) return "blocked inside DisplayNdless::present";

  if (markerPosition("TRACE_CJK_HINT_ESC_EVENT") >= 0 &&
      markerPosition("TRACE_POST_HINT_DOWN_EVENT") < 0) {
    if (markerPosition("TRACE_LOWLEVEL_POST_HINT_DOWN_SCAN") >= 0) {
      return "post-hint Down was scanned but no semantic Down event returned";
    }
    return "CJK-hint Esc completed, but the later Down was not scanned";
  }
  if (markerPosition("TRACE_POST_HINT_DOWN_EVENT") >= 0 &&
      markerPosition("TRACE_TOC_OPEN_EVENT") < 0) {
    return "CJK-hint recovery completed, but Doc did not open the TOC";
  }
  if (markerPosition("TRACE_TOC_ESC_EVENT") >= 0 &&
      markerPosition("TRACE_POST_TOC_DOWN_EVENT") < 0) {
    if (markerPosition("TRACE_LOWLEVEL_POST_TOC_DOWN_SCAN") >= 0) {
      return "post-TOC Down was scanned but no semantic Down event returned";
    }
    return "TOC Esc completed, but the later Down was not scanned";
  }
  if (markerPosition("TRACE_POST_TOC_DOWN_EVENT") >= 0 &&
      markerPosition("TRACE_CTRL_ESC_QUIT_EVENT") < 0) {
    return "both Esc recoveries completed, but Ctrl+Esc did not reach the reader";
  }
  if (markerPosition("TRACE_CTRL_ESC_QUIT_EVENT") >= 0 &&
      markerPosition("EXIT_OK") < 0) {
    if (markerPosition("TRACE_DISPLAY_SHUTDOWN_ENTER") >
        markerPosition("TRACE_DISPLAY_SHUTDOWN_RETURN")) {
      return "Ctrl+Esc reached the reader, but display shutdown blocked";
    }
    if (markerPosition("TRACE_DISPLAY_SHUTDOWN_ENTER") < 0) {
      return "Ctrl+Esc reached the reader, but viewer teardown did not finish";
    }
    return "Ctrl+Esc reached the reader, but run_reader did not return";
  }
  if (markerPosition("EXIT_OK") >= 0) return "completed through clean exit";
  return "stopped before the traced Esc reproduction sequence";
}

const diagnosis = diagnoseLastStage();
console.log(`Esc-liveness stage diagnosis: ${diagnosis}`);
writeFileSync(resolve(outdir, "stage-diagnosis.txt"), `${diagnosis}\n`);

if (result.error) {
  console.error(result.error.message);
  process.exit(2);
}
if (result.status !== 0) process.exit(result.status ?? 2);

function requireOrdered(markers) {
  let offset = 0;
  for (const marker of markers) {
    const needle = `NMARKDOWN_IT/1 ${marker}`;
    const next = serialTrace.indexOf(needle, offset);
    if (next < 0) {
      console.error(`Esc-liveness trace is missing required marker: ${needle}`);
      process.exit(1);
    }
    offset = next + needle.length;
  }
}

requireOrdered([
  "ENTER_MAIN",
  `FIXTURE_EXACT bytes=${source.length}`,
  `FIXTURE_SHA256 ${sourceSha256}`,
  "FIXTURE_READY",
  "HARFBUZZ_READY",
  "DOCUMENT_READ",
  "DOCUMENT_PARSED",
  "DOCUMENT_READY",
  "RENDER_DONE",
  "PRESENTED",
  "TRACE_LOWLEVEL_HINT_ESC_PRESS",
  "TRACE_CJK_HINT_ESC_EVENT",
  "RENDER_START",
  "TRACE_SURFACE_ENTER",
  "TRACE_SURFACE_RETURN",
  "RENDER_DONE",
  "TRACE_PRESENT_ENTER",
  "TRACE_PRESENT_RETURN",
  "PRESENTED",
  "TRACE_POLL_ENTER stage=after_hint_esc",
  "TRACE_POLL_RETURN stage=after_hint_esc",
  "TRACE_SLEEP_ENTER stage=after_hint_esc",
  "TRACE_SLEEP_RETURN stage=after_hint_esc",
  "TRACE_LOWLEVEL_POST_HINT_DOWN_SCAN",
  "TRACE_POST_HINT_DOWN_EVENT",
  "RENDER_START",
  "TRACE_SURFACE_ENTER",
  "TRACE_SURFACE_RETURN",
  "RENDER_DONE",
  "TRACE_PRESENT_ENTER",
  "TRACE_PRESENT_RETURN",
  "PRESENTED",
  "TRACE_LOWLEVEL_TOC_PRESS",
  "TRACE_TOC_OPEN_EVENT",
  "RENDER_START",
  "TRACE_SURFACE_ENTER",
  "TRACE_SURFACE_RETURN",
  "RENDER_DONE",
  "TRACE_PRESENT_ENTER",
  "TRACE_PRESENT_RETURN",
  "PRESENTED",
  "TRACE_LOWLEVEL_TOC_ESC_PRESS",
  "TRACE_TOC_ESC_EVENT",
  "RENDER_START",
  "TRACE_SURFACE_ENTER",
  "TRACE_SURFACE_RETURN",
  "RENDER_DONE",
  "TRACE_PRESENT_ENTER",
  "TRACE_PRESENT_RETURN",
  "PRESENTED",
  "TRACE_POLL_ENTER stage=after_toc_esc",
  "TRACE_POLL_RETURN stage=after_toc_esc",
  "TRACE_SLEEP_ENTER stage=after_toc_esc",
  "TRACE_SLEEP_RETURN stage=after_toc_esc",
  "TRACE_LOWLEVEL_POST_TOC_DOWN_SCAN",
  "TRACE_POST_TOC_DOWN_EVENT",
  "RENDER_START",
  "TRACE_SURFACE_ENTER",
  "TRACE_SURFACE_RETURN",
  "RENDER_DONE",
  "TRACE_PRESENT_ENTER",
  "TRACE_PRESENT_RETURN",
  "PRESENTED",
  "TRACE_CTRL_ESC_QUIT_EVENT",
  "TRACE_DISPLAY_SHUTDOWN_ENTER",
  "TRACE_DISPLAY_SHUTDOWN_RETURN",
  "TRACE_RUN_READER_RETURN",
  "EXIT_OK",
]);

// Each Esc release can occur during the first traced poll (before its return)
// or after one or more idle sleeps, depending on render time. Pin semantic
// ordering without incorrectly requiring either timing case.
requireOrdered([
  "TRACE_CJK_HINT_ESC_EVENT",
  "TRACE_LOWLEVEL_HINT_ESC_RELEASE",
  "TRACE_LOWLEVEL_POST_HINT_DOWN_SCAN",
  "TRACE_POST_HINT_DOWN_EVENT",
  "TRACE_LOWLEVEL_TOC_PRESS",
  "TRACE_TOC_OPEN_EVENT",
  "TRACE_TOC_ESC_EVENT",
  "TRACE_LOWLEVEL_TOC_ESC_RELEASE",
  "TRACE_LOWLEVEL_POST_TOC_DOWN_SCAN",
  "TRACE_POST_TOC_DOWN_EVENT",
]);

console.log(
  `Exact markdown-formula.md Esc liveness: PASS ` +
  `(${source.length} bytes, sha256=${sourceSha256}).`,
);
