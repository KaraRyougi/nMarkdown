#!/usr/bin/env node

import { createHash } from "node:crypto";
import {
  basename,
  dirname,
  resolve,
} from "node:path";
import {
  existsSync,
  mkdirSync,
  readFileSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { fileURLToPath, pathToFileURL } from "node:url";

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../..");

export const exactAssets = Object.freeze({
  document: Object.freeze({
    label: "红楼梦 TXT",
    basename: "红楼梦.txt.tns",
    size: 2_622_979,
    sha256: "50b22882745a2d17d227e4359e97fe74d7b854705d031f3e08707f836e8c2ba2",
  }),
  font: Object.freeze({
    label: "Fusion Pixel CJK font",
    basename: "fusion-pixel-12px-proportional-zh_hans.ttf.tns",
    size: 7_012_032,
    sha256: "5b27e9eb9d9dd93cff727d8919ddd2e7a482b19314b62991cb1e7806852e8734",
  }),
});

const expectedNovelIdentity = "814f6e58b9b74020";
const minimumFullPageSwipes = 30;

function fail(message) {
  throw new Error(message);
}

function records(trace, expression, map) {
  return [...trace.matchAll(expression)].map((match) => ({
    position: match.index,
    end: match.index + match[0].length,
    ...map(match),
  }));
}

function singleton(items, label) {
  if (items.length !== 1) {
    fail(`expected exactly one ${label} marker, observed ${items.length}`);
  }
  return items[0];
}

function integer(value, label) {
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed)) {
    fail(`${label} is not a safe integer: ${value}`);
  }
  return parsed;
}

function requireOrdered(items, label) {
  for (let index = 1; index < items.length; ++index) {
    if (!(items[index - 1].position < items[index].position)) {
      fail(`${label} markers are not strictly ordered at item ${index + 1}`);
    }
  }
}

function marker(trace, expression, label) {
  const match = expression.exec(trace);
  expression.lastIndex = 0;
  if (match === null) fail(`missing ${label} marker`);
  return {
    position: match.index,
    end: match.index + match[0].length,
    match,
  };
}

export function verifyExactAsset(path, expected) {
  const absolutePath = resolve(path);
  if (!existsSync(absolutePath)) {
    fail(`${expected.label} is missing: ${absolutePath}`);
  }
  if (basename(absolutePath) !== expected.basename) {
    fail(
      `${expected.label} must be named ${expected.basename}, got ` +
      basename(absolutePath),
    );
  }
  const info = statSync(absolutePath);
  if (!info.isFile()) {
    fail(`${expected.label} is not a regular file: ${absolutePath}`);
  }
  if (info.size !== expected.size) {
    fail(
      `${expected.label} has ${info.size} bytes; expected ${expected.size}`,
    );
  }
  const sha256 = createHash("sha256")
    .update(readFileSync(absolutePath))
    .digest("hex");
  if (sha256 !== expected.sha256) {
    fail(
      `${expected.label} SHA-256 mismatch: ${sha256}; ` +
      `expected ${expected.sha256}`,
    );
  }
  return {
    path: absolutePath,
    basename: expected.basename,
    size: info.size,
    sha256,
  };
}

export function verifySerialTrace(trace) {
  if (typeof trace !== "string" || trace.length === 0) {
    fail("serial trace is empty");
  }

  const forbidden = [
    /NMARKDOWN_IT\/1 REAL_NOVEL_MISMATCH\b/,
    /NMARKDOWN_IT\/1 REAL_NOVEL_PROGRESS_FAIL\b/,
    /NMARKDOWN_IT\/1 REAL_NOVEL_30_SWIPE_INCOMPLETE\b/,
    /NMARKDOWN_IT\/1 REAL_NOVEL_ASSETS_TIMEOUT\b/,
    /NMARKDOWN_IT\/1 (?:FIXTURE|FONT|HARFBUZZ|DOCUMENT)_FAIL\b/,
    /NMARKDOWN_IT\/1 EXIT_ERROR\b/,
    /FIREBIRD_GUI_TEST\/1 FAIL\b/,
    /FIXME:\s*TPAD read 040[c-f]\b/i,
  ];
  for (const expression of forbidden) {
    const match = expression.exec(trace);
    if (match !== null) {
      fail(`failure marker present: ${match[0]}`);
    }
  }

  const startup = [
    marker(trace, /NMARKDOWN_IT\/1 ENTER_MAIN\b/g, "ENTER_MAIN"),
    marker(
      trace,
      /NMARKDOWN_IT\/1 RELATIVE_PATHS_READY\b/g,
      "RELATIVE_PATHS_READY",
    ),
    marker(trace, /NMARKDOWN_IT\/1 FIXTURE_READY\b/g, "FIXTURE_READY"),
    marker(
      trace,
      /NMARKDOWN_IT\/1 REAL_NOVEL_ASSET_WAIT\b/g,
      "REAL_NOVEL_ASSET_WAIT",
    ),
    marker(
      trace,
      /NMARKDOWN_IT\/1 REAL_NOVEL_ASSETS_READY\b/g,
      "REAL_NOVEL_ASSETS_READY",
    ),
    marker(
      trace,
      /NMARKDOWN_IT\/1 CLOCK_(?:HARDWARE|LOGICAL)\b/g,
      "calculator clock",
    ),
    marker(trace, /NMARKDOWN_IT\/1 VIEWER_READY\b/g, "VIEWER_READY"),
    marker(trace, /NMARKDOWN_IT\/1 HARFBUZZ_READY\b/g, "HARFBUZZ_READY"),
    marker(trace, /NMARKDOWN_IT\/1 FONT_READY\b/g, "FONT_READY"),
    marker(
      trace,
      /NMARKDOWN_IT\/1 DOCUMENT_LOAD_START\b/g,
      "DOCUMENT_LOAD_START",
    ),
    marker(
      trace,
      /NMARKDOWN_IT\/1 DOCUMENT_PROBED\b/g,
      "DOCUMENT_PROBED",
    ),
    marker(trace, /NMARKDOWN_IT\/1 DOCUMENT_READ\b/g, "DOCUMENT_READ"),
  ];
  requireOrdered(startup, "startup");

  const exact = singleton(records(
    trace,
    /NMARKDOWN_IT\/1 REAL_NOVEL_EXACT bytes=(\d+) identity=([0-9a-fA-F]{16}) streamed=([01])/g,
    (match) => ({
      bytes: integer(match[1], "REAL_NOVEL_EXACT bytes"),
      identity: match[2].toLowerCase(),
      streamed: match[3] === "1",
    }),
  ), "REAL_NOVEL_EXACT");
  if (exact.bytes !== exactAssets.document.size ||
      exact.identity !== expectedNovelIdentity ||
      !exact.streamed) {
    fail(
      "REAL_NOVEL_EXACT does not identify the required streamed document: " +
      JSON.stringify({
        bytes: exact.bytes,
        identity: exact.identity,
        streamed: exact.streamed,
      }),
    );
  }

  const afterExactStartup = [
    exact,
    marker(
      trace,
      /NMARKDOWN_IT\/1 DOCUMENT_PARSED\b/g,
      "DOCUMENT_PARSED",
    ),
    marker(
      trace,
      /NMARKDOWN_IT\/1 DOCUMENT_READY\b/g,
      "DOCUMENT_READY",
    ),
    marker(trace, /NMARKDOWN_IT\/1 RENDER_START\b/g, "initial RENDER_START"),
    marker(trace, /NMARKDOWN_IT\/1 RENDER_DONE\b/g, "initial RENDER_DONE"),
    marker(trace, /NMARKDOWN_IT\/1 PRESENTED\b/g, "initial PRESENTED"),
  ];
  if (!(startup.at(-1).position < exact.position)) {
    fail("REAL_NOVEL_EXACT appeared before DOCUMENT_READ");
  }
  requireOrdered(afterExactStartup, "document-open");

  const progressStart = singleton(records(
    trace,
    /NMARKDOWN_IT\/1 REAL_NOVEL_PROGRESS_START offset=(\d+) page=(\d+)/g,
    (match) => ({
      offset: integer(match[1], "initial source offset"),
      page: integer(match[2], "initial page"),
    }),
  ), "REAL_NOVEL_PROGRESS_START");
  if (!(afterExactStartup.at(-1).position < progressStart.position)) {
    fail("REAL_NOVEL_PROGRESS_START appeared before the initial presentation");
  }
  if (progressStart.offset !== 0 || progressStart.page !== 1) {
    fail(
      `real-novel progress must start at offset 0/page 1, got ` +
      `${progressStart.offset}/page ${progressStart.page}`,
    );
  }
  const guiStart = singleton(records(
    trace,
    /FIREBIRD_GUI_TEST\/1 START\b/g,
    () => ({}),
  ), "FIREBIRD_GUI_TEST START");
  if (!(progressStart.position < guiStart.position)) {
    fail("Firebird GUI swipe driver started before document progress tracking");
  }

  const passes = records(
    trace,
    /NMARKDOWN_IT\/1 REAL_NOVEL_30_SWIPE_PASS requests=(\d+) commits=(\d+) presented=(\d+) offset=(\d+) page=(\d+)/g,
    (match) => ({
      requests: integer(match[1], "PASS requests"),
      commits: integer(match[2], "PASS commits"),
      presented: integer(match[3], "PASS presented"),
      offset: integer(match[4], "PASS offset"),
      page: integer(match[5], "PASS page"),
    }),
  );
  const pass = singleton(passes, "REAL_NOVEL_30_SWIPE_PASS");
  const guiPass = singleton(records(
    trace,
    /FIREBIRD_GUI_TEST\/1 PASS\b/g,
    () => ({}),
  ), "FIREBIRD_GUI_TEST PASS");
  if (!(pass.position < guiPass.position)) {
    fail("Firebird GUI PASS appeared before the application PASS");
  }

  const requests = records(
    trace,
    /NMARKDOWN_IT\/1 REAL_NOVEL_SWIPE_REQUEST count=(\d+)/g,
    (match) => ({ count: integer(match[1], "swipe request count") }),
  ).filter((item) => item.position < pass.position);
  const txtRequests = records(
    trace,
    /NMARKDOWN_IT\/1 TXT_PAGE_REQUEST direction=(-?\d+) before=(\d+) after=(\d+) moved=([01]) error=([01])/g,
    (match) => ({
      direction: integer(match[1], "TXT page direction"),
      before: integer(match[2], "TXT request before offset"),
      after: integer(match[3], "TXT request after offset"),
      moved: match[4] === "1",
      error: match[5] === "1",
    }),
  ).filter((item) => item.position < pass.position);
  const lowLevelCommits = records(
    trace,
    /NMARKDOWN_IT\/1 TXT_PAGE_COMMIT source=(input|deferred) before=(\d+) after=(\d+)/g,
    (match) => ({
      source: match[1],
      before: integer(match[2], "TXT commit before offset"),
      after: integer(match[3], "TXT commit after offset"),
    }),
  ).filter((item) => item.position < pass.position);
  const commits = records(
    trace,
    /NMARKDOWN_IT\/1 REAL_NOVEL_PAGE_COMMIT count=(\d+) before=(\d+) after=(\d+) page=(\d+)/g,
    (match) => ({
      count: integer(match[1], "page commit count"),
      before: integer(match[2], "page commit before offset"),
      after: integer(match[3], "page commit after offset"),
      page: integer(match[4], "page commit page"),
    }),
  ).filter((item) => item.position < pass.position);
  const visible = records(
    trace,
    /NMARKDOWN_IT\/1 REAL_NOVEL_VISIBLE count=(\d+) offset=(\d+) page=(\d+)/g,
    (match) => ({
      count: integer(match[1], "visible count"),
      offset: integer(match[2], "visible offset"),
      page: integer(match[3], "visible page"),
    }),
  ).filter((item) => item.position < pass.position);
  const guiSwipes = records(
    trace,
    /FIREBIRD_GUI_TEST\/1 SWIPE_SENT count=(\d+)/g,
    (match) => ({ count: integer(match[1], "Firebird GUI swipe count") }),
  );

  const completed = pass.commits;
  if (pass.requests < minimumFullPageSwipes ||
      completed < minimumFullPageSwipes ||
      pass.presented < minimumFullPageSwipes) {
    fail(
      `PASS reports fewer than ${minimumFullPageSwipes} complete swipes: ` +
      `${pass.requests}/${pass.commits}/${pass.presented}`,
    );
  }
  if (pass.requests !== completed || pass.presented !== completed) {
    fail(
      "PASS contains uncommitted or unpresented requests: " +
      `${pass.requests}/${pass.commits}/${pass.presented}`,
    );
  }

  const groups = [
    ["swipe requests", requests],
    ["TXT page requests", txtRequests],
    ["TXT page commits", lowLevelCommits],
    ["page commits", commits],
    ["visible commits", visible],
  ];
  for (const [label, items] of groups) {
    if (items.length !== completed) {
      fail(
        `${label} count ${items.length} does not match PASS commits ${completed}`,
      );
    }
    requireOrdered(items, label);
  }
  if (guiSwipes.length !== completed) {
    fail(
      `Firebird GUI swipe count ${guiSwipes.length} does not match ` +
      `PASS commits ${completed}`,
    );
  }
  requireOrdered(guiSwipes, "Firebird GUI swipes");
  if (!(guiStart.position < requests[0].position)) {
    fail("document input began before the Firebird GUI swipe driver");
  }

  const renderStarts = records(
    trace,
    /NMARKDOWN_IT\/1 RENDER_START\b/g,
    () => ({}),
  );
  const renderDone = records(
    trace,
    /NMARKDOWN_IT\/1 RENDER_DONE\b/g,
    () => ({}),
  );
  const presented = records(
    trace,
    /NMARKDOWN_IT\/1 PRESENTED\b/g,
    () => ({}),
  );

  let previousOffset = progressStart.offset;
  let previousPage = progressStart.page;
  for (let index = 0; index < completed; ++index) {
    const expectedCount = index + 1;
    const request = requests[index];
    const txtRequest = txtRequests[index];
    const lowCommit = lowLevelCommits[index];
    const commit = commits[index];
    const shown = visible[index];
    const guiSwipe = guiSwipes[index];

    if (request.count !== expectedCount ||
        commit.count !== expectedCount ||
        shown.count !== expectedCount ||
        guiSwipe.count !== expectedCount) {
      fail(
        `transaction ${expectedCount} has non-sequential counts: ` +
        `${request.count}/${commit.count}/${shown.count}/${guiSwipe.count}`,
      );
    }
    const nextRequestPosition = requests[index + 1]?.position ?? pass.position;
    if (!(request.position < txtRequest.position &&
          txtRequest.position < nextRequestPosition)) {
      fail(
        `transaction ${expectedCount} does not contain exactly one ordered ` +
        "TXT_PAGE_REQUEST immediately following its swipe request",
      );
    }
    const nextGuiRequestPosition =
      requests[index + 1]?.position ?? trace.length;
    if (!(request.position < guiSwipe.position &&
          guiSwipe.position < nextGuiRequestPosition)) {
      fail(
        `transaction ${expectedCount} lacks one GUI touchpad swipe between ` +
        "this request and the next request",
      );
    }
    if (txtRequest.direction !== 1 || txtRequest.error ||
        txtRequest.after < txtRequest.before ||
        txtRequest.moved !== (txtRequest.after > txtRequest.before)) {
      fail(
        `transaction ${expectedCount} has an invalid TXT page request: ` +
        JSON.stringify(txtRequest),
      );
    }
    if (!(request.position < lowCommit.position &&
          lowCommit.position < commit.position &&
          commit.position < shown.position)) {
      fail(
        `transaction ${expectedCount} is not ordered ` +
        "request -> TXT commit -> app commit -> visible",
      );
    }
    if (index + 1 < completed &&
        !(shown.position < commits[index + 1].position)) {
      fail(
        `transaction ${expectedCount + 1} committed before transaction ` +
        `${expectedCount} became visible`,
      );
    }
    if (commit.before !== previousOffset ||
        commit.after <= commit.before ||
        commit.page !== previousPage + 1) {
      fail(
        `transaction ${expectedCount} did not strictly advance offset/page: ` +
        JSON.stringify({
          expectedBefore: previousOffset,
          expectedPage: previousPage + 1,
          observed: commit,
        }),
      );
    }
    if (lowCommit.before !== commit.before ||
        lowCommit.after !== commit.after) {
      fail(
        `transaction ${expectedCount} low-level/app commit disagreement: ` +
        JSON.stringify({ lowCommit, commit }),
      );
    }
    if (txtRequest.moved) {
      if (lowCommit.source !== "input" ||
          txtRequest.before !== commit.before ||
          txtRequest.after !== commit.after) {
        fail(
          `transaction ${expectedCount} immediate commit is inconsistent`,
        );
      }
    } else if (lowCommit.source !== "deferred") {
      fail(
        `transaction ${expectedCount} deferred request committed as ` +
        lowCommit.source,
      );
    }
    if (shown.offset !== commit.after || shown.page !== commit.page) {
      fail(
        `transaction ${expectedCount} visible marker does not match commit`,
      );
    }

    const renderStart = renderStarts.find(
      (item) => item.position > commit.position &&
                item.position < shown.position,
    );
    const rendered = renderDone.find(
      (item) => renderStart !== undefined &&
                item.position > renderStart.position &&
                item.position < shown.position,
    );
    const presentation = presented.find(
      (item) => rendered !== undefined &&
                item.position > rendered.position &&
                item.position < shown.position,
    );
    if (renderStart === undefined || rendered === undefined ||
        presentation === undefined) {
      fail(
        `transaction ${expectedCount} lacks ordered ` +
        "RENDER_START -> RENDER_DONE -> PRESENTED evidence",
      );
    }

    previousOffset = commit.after;
    previousPage = commit.page;
  }

  if (pass.offset !== previousOffset || pass.page !== previousPage) {
    fail(
      "PASS final offset/page does not match the last visible commit: " +
      `${pass.offset}/${pass.page} versus ${previousOffset}/${previousPage}`,
    );
  }

  const postPassProgress = trace.slice(pass.end).match(
    /NMARKDOWN_IT\/1 (?:REAL_NOVEL_(?:SWIPE_REQUEST|PAGE_COMMIT|VISIBLE|PROGRESS_FAIL)|TXT_PAGE_(?:REQUEST|COMMIT))/,
  );
  if (postPassProgress !== null) {
    fail(`progress marker appeared after final PASS: ${postPassProgress[0]}`);
  }

  const tpadPaddingReads = (
    trace.match(/FIXME:\s*TPAD read 040[c-f]\b/gi) ?? []
  ).length;
  return {
    status: "pass",
    minimumRequiredSwipes: minimumFullPageSwipes,
    requests: pass.requests,
    commits: pass.commits,
    visible: pass.presented,
    guiSwipes: guiSwipes.length,
    startOffset: progressStart.offset,
    finalOffset: pass.offset,
    startPage: progressStart.page,
    finalPage: pass.page,
    streamedDocument: exact.streamed,
    sampledIdentity: exact.identity,
    tpadPaddingReads,
  };
}

function parseArguments(argv) {
  const options = {
    serial: process.env.FIREBIRD_REAL_NOVEL_SERIAL_LOG ??
      resolve(projectRoot, "build/firebird-real-novel-gui/serial.log"),
    document: process.env.REAL_NOVEL_DOCUMENT ??
      "/Users/ryougi/Downloads/红楼梦.txt.tns",
    font: process.env.REAL_NOVEL_FONT ??
      "/Users/ryougi/Downloads/fusion-pixel-12px-proportional-zh_hans.ttf.tns",
    output: "",
  };
  for (const argument of argv) {
    if (argument === "--help" || argument === "-h") {
      return { ...options, help: true };
    }
    if (argument.startsWith("--serial=")) {
      options.serial = resolve(argument.slice("--serial=".length));
    } else if (argument.startsWith("--document=")) {
      options.document = resolve(argument.slice("--document=".length));
    } else if (argument.startsWith("--font=")) {
      options.font = resolve(argument.slice("--font=".length));
    } else if (argument.startsWith("--output=")) {
      options.output = resolve(argument.slice("--output=".length));
    } else if (!argument.startsWith("-") &&
               options.serial.endsWith("/serial.log")) {
      options.serial = resolve(argument);
    } else {
      fail(`unknown argument: ${argument}`);
    }
  }
  return options;
}

function usage() {
  return [
    "Usage:",
    "  node tools/firebird/verify-real-novel-gui.mjs [SERIAL.log]",
    "    [--serial=SERIAL.log]",
    "    [--document=/path/to/红楼梦.txt.tns]",
    "    [--font=/path/to/fusion-pixel-12px-proportional-zh_hans.ttf.tns]",
    "    [--output=verification.json]",
    "",
    "The default host inputs are the exact files under ~/Downloads. The",
    "default trace is build/firebird-real-novel-gui/serial.log.",
  ].join("\n");
}

export function runVerification(options) {
  const document = verifyExactAsset(options.document, exactAssets.document);
  const font = verifyExactAsset(options.font, exactAssets.font);
  const serialPath = resolve(options.serial);
  if (!existsSync(serialPath)) {
    fail(`Firebird GUI serial trace is missing: ${serialPath}`);
  }
  const trace = readFileSync(serialPath, "utf8");
  const serial = verifySerialTrace(trace);
  return {
    status: "pass",
    assets: { document, font },
    serial: {
      path: serialPath,
      ...serial,
    },
  };
}

async function main() {
  try {
    const options = parseArguments(process.argv.slice(2));
    if (options.help) {
      console.log(usage());
      return;
    }
    const result = runVerification(options);
    const json = `${JSON.stringify(result, null, 2)}\n`;
    if (options.output) {
      mkdirSync(dirname(options.output), { recursive: true });
      writeFileSync(options.output, json);
    }
    process.stdout.write(json);
  } catch (error) {
    console.error(
      `nMarkdown real-novel GUI verification: FAIL: ` +
      (error instanceof Error ? error.message : String(error)),
    );
    process.exitCode = 1;
  }
}

if (process.argv[1] !== undefined &&
    pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  await main();
}
