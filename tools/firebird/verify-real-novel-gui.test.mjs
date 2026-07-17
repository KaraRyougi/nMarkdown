#!/usr/bin/env node

import assert from "node:assert/strict";

import {
  verifySerialTrace,
} from "./verify-real-novel-gui.mjs";

function validTrace(count = 30) {
  const lines = [
    "NMARKDOWN_IT/1 ENTER_MAIN",
    "NMARKDOWN_IT/1 RELATIVE_PATHS_READY",
    "NMARKDOWN_IT/1 FIXTURE_READY",
    "NMARKDOWN_IT/1 REAL_NOVEL_ASSET_WAIT",
    "NMARKDOWN_IT/1 REAL_NOVEL_ASSETS_READY",
    "NMARKDOWN_IT/1 CLOCK_LOGICAL",
    "NMARKDOWN_IT/1 VIEWER_READY",
    "NMARKDOWN_IT/1 HARFBUZZ_READY",
    "NMARKDOWN_IT/1 FONT_READY",
    "NMARKDOWN_IT/1 DOCUMENT_LOAD_START",
    "NMARKDOWN_IT/1 DOCUMENT_PROBED",
    "NMARKDOWN_IT/1 DOCUMENT_READ",
    "NMARKDOWN_IT/1 REAL_NOVEL_EXACT bytes=2622979 " +
      "identity=814f6e58b9b74020 streamed=1",
    "NMARKDOWN_IT/1 DOCUMENT_PARSED",
    "NMARKDOWN_IT/1 DOCUMENT_READY",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 REAL_NOVEL_PROGRESS_START offset=0 page=1",
    "FIREBIRD_GUI_TEST/1 START",
  ];
  let offset = 0;
  for (let index = 1; index <= count; ++index) {
    const next = offset + 400 + index;
    lines.push(
      `NMARKDOWN_IT/1 REAL_NOVEL_SWIPE_REQUEST count=${index}`,
      `NMARKDOWN_IT/1 TXT_PAGE_REQUEST direction=1 before=${offset} ` +
        `after=${offset} moved=0 error=0`,
      `NMARKDOWN_IT/1 TXT_PAGE_COMMIT source=deferred before=${offset} ` +
        `after=${next}`,
      `NMARKDOWN_IT/1 REAL_NOVEL_PAGE_COMMIT count=${index} ` +
        `before=${offset} after=${next} page=${index + 1}`,
      "NMARKDOWN_IT/1 RENDER_START",
      "NMARKDOWN_IT/1 RENDER_DONE",
      "NMARKDOWN_IT/1 PRESENTED",
      `NMARKDOWN_IT/1 REAL_NOVEL_VISIBLE count=${index} ` +
        `offset=${next} page=${index + 1}`,
      `FIREBIRD_GUI_TEST/1 SWIPE_SENT count=${index}`,
    );
    offset = next;
  }
  lines.push(
    `NMARKDOWN_IT/1 REAL_NOVEL_30_SWIPE_PASS requests=${count} ` +
      `commits=${count} presented=${count} offset=${offset} page=${count + 1}`,
    "FIREBIRD_GUI_TEST/1 PASS",
  );
  return `${lines.join("\n")}\n`;
}

function mustReject(trace, description) {
  assert.throws(
    () => verifySerialTrace(trace),
    Error,
    description,
  );
}

const result = verifySerialTrace(validTrace());
assert.equal(result.status, "pass");
assert.equal(result.requests, 30);
assert.equal(result.commits, 30);
assert.equal(result.visible, 30);
assert.equal(result.guiSwipes, 30);
assert.equal(result.startPage, 1);
assert.equal(result.finalPage, 31);
assert.equal(result.tpadPaddingReads, 0);

const longer = verifySerialTrace(validTrace(32));
assert.equal(longer.requests, 32);
assert.equal(longer.finalPage, 33);

mustReject(
  validTrace().replace(
    "REAL_NOVEL_PAGE_COMMIT count=12 before=4466 after=4878 page=13",
    "REAL_NOVEL_PAGE_COMMIT count=12 before=4466 after=4466 page=13",
  ),
  "a non-increasing source offset must fail",
);
mustReject(
  validTrace().replace(
    "NMARKDOWN_IT/1 REAL_NOVEL_VISIBLE count=20 offset=8210 page=21\n",
    "",
  ),
  "a missing visible page must fail",
);
mustReject(
  validTrace().replace(
    "NMARKDOWN_IT/1 RENDER_DONE\nNMARKDOWN_IT/1 PRESENTED\n" +
      "NMARKDOWN_IT/1 REAL_NOVEL_VISIBLE count=8",
    "NMARKDOWN_IT/1 PRESENTED\nNMARKDOWN_IT/1 RENDER_DONE\n" +
      "NMARKDOWN_IT/1 REAL_NOVEL_VISIBLE count=8",
  ),
  "presentation ordering must be enforced",
);
mustReject(
  validTrace().replace(
    "NMARKDOWN_IT/1 REAL_NOVEL_30_SWIPE_PASS",
    "NMARKDOWN_IT/1 REAL_NOVEL_PROGRESS_FAIL requests=30 commits=30 " +
      "before=1 after=1 page_before=30 page_after=30\n" +
      "NMARKDOWN_IT/1 REAL_NOVEL_30_SWIPE_PASS",
  ),
  "failure markers must override a later PASS",
);
mustReject(
  validTrace().replace(
    "FIREBIRD_GUI_TEST/1 SWIPE_SENT count=17\n",
    "",
  ),
  "a missing GUI swipe must fail",
);
mustReject(
  validTrace().replace(
    "FIREBIRD_GUI_TEST/1 START",
    "FIREBIRD_GUI_TEST/1 START\nFIXME: TPAD read 040c",
  ),
  "TPAD padding warning regressions must fail",
);

console.log("verify-real-novel-gui self-test: PASS");
