#!/usr/bin/env node

import { existsSync, readFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const candidates = [
  process.env.POCKETJS_NSPIRE,
  resolve(homedir(), "Documents/PocketJS-NSpire"),
  resolve(projectRoot, "../PocketJS-NSpire"),
].filter(Boolean);
const pocketRoot = candidates.find((candidate) =>
  existsSync(resolve(candidate, "scripts/nspire-firebird.ts")),
);

if (!pocketRoot) {
  console.error(
    "PocketJS-NSpire Firebird harness not found; set POCKETJS_NSPIRE to its checkout.",
  );
  process.exit(2);
}

const mathFixture = process.env.FIREBIRD_MATH_FIXTURE === "1";
const mathReviewFixture = process.env.FIREBIRD_MATH_REVIEW_FIXTURE === "1";
const oversizedFormulaFixture =
  process.env.FIREBIRD_OVERSIZED_FORMULA_FIXTURE === "1";
const scrollSwipeFixture = process.env.FIREBIRD_SCROLL_SWIPE_FIXTURE === "1";
const pageFixture = process.env.FIREBIRD_PAGE_FIXTURE === "1";
const formatFixture = process.env.FIREBIRD_FORMAT_FIXTURE === "1";
const tocFixture = process.env.FIREBIRD_TOC_FIXTURE === "1";
const browserCancelFixture = process.env.FIREBIRD_BROWSER_CANCEL_FIXTURE === "1";
const themeFixture = process.env.FIREBIRD_THEME_FIXTURE === "1";
const stateFixture = process.env.FIREBIRD_STATE_FIXTURE === "1";
const fontMenuFixture = process.env.FIREBIRD_FONT_MENU_FIXTURE === "1";
const captureOnly = process.env.FIREBIRD_CAPTURE_ONLY === "1";
const program = resolve(
  projectRoot,
  fontMenuFixture
    ? "build/ndless-firebird-font-menu/nmarkdown-firebird-font-menu.tns"
    : stateFixture
    ? "build/ndless-firebird-state/nmarkdown-firebird-state.tns"
    : themeFixture
    ? "build/ndless-firebird-theme/nmarkdown-firebird-theme.tns"
    : browserCancelFixture
    ? "build/ndless-firebird-browser-cancel/nmarkdown-firebird-browser-cancel.tns"
    : tocFixture
    ? "build/ndless-firebird-toc/nmarkdown-firebird-toc.tns"
    : formatFixture
    ? "build/ndless-firebird-formats/nmarkdown-firebird-formats.tns"
    : pageFixture
    ? "build/ndless-firebird-page/nmarkdown-firebird-page.tns"
    : oversizedFormulaFixture
    ? "build/ndless-firebird-oversized-formula/nmarkdown-firebird-oversized-formula.tns"
    : scrollSwipeFixture
    ? "build/ndless-firebird-scroll-swipe/nmarkdown-firebird-scroll-swipe.tns"
    : mathReviewFixture
    ? "build/ndless-firebird-math-review/nmarkdown-firebird-math-review.tns"
    : mathFixture
    ? "build/ndless-firebird-math/nmarkdown-firebird-math.tns"
    : "build/ndless-firebird/nmarkdown-firebird-integration.tns",
);
if (!existsSync(program)) {
  console.error(`Firebird integration build is missing: ${program}`);
  process.exit(2);
}

const outdir = resolve(
  projectRoot,
  fontMenuFixture
    ? "build/firebird-font-menu"
    : stateFixture
    ? "build/firebird-state"
    : themeFixture
    ? "build/firebird-theme"
    : browserCancelFixture
    ? "build/firebird-browser-cancel"
    : tocFixture
    ? "build/firebird-toc"
    : formatFixture
    ? "build/firebird-formats"
    : pageFixture
    ? "build/firebird-page"
    : oversizedFormulaFixture
    ? "build/firebird-oversized-formula"
    : scrollSwipeFixture
    ? "build/firebird-scroll-swipe"
    : mathReviewFixture
    ? "build/firebird-math-review"
    : mathFixture
      ? "build/firebird-math"
      : "build/firebird",
);
const fontFixture = process.env.FIREBIRD_FONT_FIXTURE === "1" || formatFixture;
const args = [
  resolve(pocketRoot, "scripts/nspire-firebird.ts"),
  "test",
  `--program=${program}`,
  // Keep the startup filename used by the reference harness so the tested
  // TI-OS document ordering and activation tape remain identical.
  "--remote=/ndless/startup/pocketjs-integration.tns",
  `--outdir=${outdir}`,
  "--timeout-ms=180000",
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
];

if (!browserCancelFixture && !themeFixture && !stateFixture &&
    !fontMenuFixture) {
  const finalFrameHasReadingProgress =
    tocFixture || scrollSwipeFixture || pageFixture;
  args.push(
    // Reviewed RGB565 pixels cover app chrome and the document paper.
    `--expect=0,0,${finalFrameHasReadingProgress ? "0x3d4d" : "0xbe39"}`,
    "--expect=0,2,0x1969",
    "--expect=0,18,0xffff",
    ...(tocFixture || scrollSwipeFixture ? [] : ["--expect=8,18,0xffff"]),
    "--expect=319,20,0xffff",
    // The final two rows of the paper body are a guaranteed text-free inset.
    "--expect=0,238,0xffff",
    "--expect=319,239,0xffff",
  );
}

if (themeFixture) {
  args.push(
    // PocketJS cannot press Menu or Ctrl+T here. This fixture remaps Doc from
    // its production Sections action to the same OpenSettings event.
    "--key=doc",
    "--key=right",
    "--key=enter",
    "--key=wait",
    "--key=down",
    "--key=wait",
    "--key=doc",
    "--key=wait",
    // Settings must ignore both a physical swipe and the following native
    // click report generated by the fixture probe.
    "--key=touch-center",
    "--key=touch-up",
    "--key=touch-release",
    "--key=touch-center",
    "--key=touch-release",
    "--key=wait",
    // Keyboard Down selects Font size and Right changes it from 15 px to
    // 16 px. Enter would apply the settings and close the overlay.
    "--key=down",
    "--key=right",
    "--key=wait",
    // The final frame is a responsive dark Settings overlay with Font size
    // selected and changed from 15 px to 16 px.
    "--expect=0,0,0x45ef",
    "--expect=0,2,0x08c6",
    // The modal scrim darkens the document body outside the settings panel.
    "--expect=0,20,0x10c4",
    "--expect=319,20,0x10c4",
    "--expect=14,50,0x4d3c",
    "--expect=22,44,0x2146",
    "--expect=22,64,0x4d3c",
    "--expect=23,70,0xffff",
    "--expect=15,70,0x2146",
    "--expect=0,238,0x10c4",
    "--expect=319,239,0x10c4",
  );
}

if (pageFixture) {
  // Each vertical swipe is one context-preserving screen step in Horizontal
  // Scroll mode. Bounce forward -> back -> forward so both directions cross the
  // native input classifier and leave the reviewed frame after one forward
  // step. The sampler also reports vertical deltas, but Viewer must ignore those
  // in this mode so no contact is applied twice. PocketJS names directions in screen
  // space; InputNdless converts Firebird's inverted native report.y first.
  args.push(
    "--key=touch-center",
    "--key=wait",
    "--key=touch-up",
    "--key=wait",
    "--key=touch-release",
    "--key=wait",
    "--key=touch-center",
    "--key=wait",
    "--key=touch-down",
    "--key=wait",
    "--key=touch-release",
    "--key=wait",
    "--key=touch-center",
    "--key=wait",
    "--key=touch-up",
    "--key=wait",
    "--key=touch-release",
    "--key=wait",
  );
}

if (oversizedFormulaFixture) {
  args.push(
    // Enter targets the visible oversized formula even though a heading and
    // prose precede it. Right performs a fine local pan.
    "--key=enter",
    "--key=wait",
    "--key=right",
    "--key=wait",
    // A persistent center contact followed by a left move is a SwipeLeft
    // gesture. In formula focus it pans by most of a viewport rather than
    // scrolling the document.
    "--key=touch-center",
    "--key=wait",
    "--key=touch-left",
    "--key=wait",
    "--key=touch-release",
    "--key=wait",
    // A later fine pan proves input and repaint remain live after the swipe;
    // the final Right returns to the exact far-right clamp so the capture can
    // actually prove the annotation and tag lanes' right alignment.
    "--key=left",
    "--key=wait",
    "--key=right",
    "--key=wait",
    // At the true maximum pan, the focus border is x=310 and the separate
    // right-aligned tag lane's closing-parenthesis ink ends at x=307. The
    // following white pixels prove neither a clipped tag nor the old x=313
    // continuation marker survives. Annotation-column equality is pinned by
    // the exact native-layout regression and reviewed in this final frame.
    "--expect=310,141,0x3c9a",
    "--expect=311,141,0xffff",
    "--expect=310,208,0x3c9a",
    "--expect=311,208,0xffff",
    "--expect=307,150,0x8472",
    "--expect=308,150,0xffff",
    "--expect=307,171,0x8431",
    "--expect=308,171,0xffff",
    "--expect=313,150,0xffff",
  );
}

if (scrollSwipeFixture) {
  args.push(
    // Establish a small, verified nonzero Scroll-mode position first.
    "--key=down",
    "--key=wait",
    // Moving the finger left must reveal earlier document content.
    "--key=touch-center",
    "--key=wait",
    "--key=touch-left",
    "--key=wait",
    "--key=touch-release",
    "--key=wait",
    // A new contact moving right must reveal later content.
    "--key=touch-center",
    "--key=wait",
    "--key=touch-right",
    "--key=wait",
    "--key=touch-release",
    "--key=wait",
    // A later keypad event must still repaint, proving post-swipe liveness.
    "--key=down",
    "--key=wait",
    // The default Natural setting moves content with the finger: an upward
    // drag advances and a downward drag returns.
    "--key=touch-center",
    "--key=wait",
    "--key=touch-up",
    "--key=wait",
    "--key=touch-release",
    "--key=wait",
    "--key=touch-center",
    "--key=wait",
    "--key=touch-down",
    "--key=wait",
    "--key=touch-release",
    "--key=wait",
  );
}

if (tocFixture) {
  args.push(
    // Doc is the production key for the current document's section list.
    "--key=doc",
    "--key=down",
    "--key=enter",
    "--key=wait",
    "--key=doc",
    "--key=down",
    "--key=enter",
    "--key=wait",
    // Jump backward after the two forward destinations. This stresses the
    // lazy prefix-height map in both directions within one app session.
    "--key=doc",
    "--key=up",
    "--key=enter",
    "--key=wait",
    // A fourth open/close proves that input and rendering remain live after
    // all three distinct navigation transitions. Wait for Esc to be fully
    // released, then send a later Down key; this is the physical sequence
    // reported to freeze on hardware after the document reappears.
    "--key=doc",
    "--key=esc",
    "--key=wait",
    "--key=down",
    "--key=wait",
  );
}

if (browserCancelFixture) {
  // Esc cancels the startup browser. The application must return normally;
  // the final wait gives TI-OS time to resume before validation.
  // The framebuffer has returned to TI-OS by this point. Its dark system
  // header supplies the custom-program assertion required by PocketJS.
  args.push("--key=esc", "--key=wait", "--expect=0,0,0x2985");
}

if (stateFixture) {
  args.push(
    // The fixture-only Doc alias opens Settings. Changing Dark mode performs
    // the first state write; normal Esc exit then replaces that existing file.
    "--key=doc",
    "--key=enter",
    "--key=wait",
    "--key=esc",
    "--key=esc",
    "--key=wait",
    // TI-OS can raise its post-Ndless low-memory notice after the program
    // returns. Esc safely dismisses it and is inert on the home screen, giving
    // this exit fixture one deterministic final frame in both cases.
    "--key=esc",
    "--key=wait",
    "--expect=0,0,0x31a6",
    "--expect=319,20,0x31a6",
    "--expect=0,45,0x0000",
    "--expect=20,55,0x23d9",
    "--expect=300,100,0x0000",
    "--expect=319,239,0x0000",
  );
}

if (fontMenuFixture) {
  args.push(
    // PocketJS cannot press Menu or hold Ctrl+T as a chord. Only this fixture
    // remaps Doc to OpenSettings; production uses Menu or Ctrl+T.
    "--key=doc",
    // Move from Theme to the twelfth Settings row: Fonts.
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=enter",
    // The installed-file list contains one Sarasa fixture. Open its detail.
    "--key=enter",
    // Use metadata suggestions (CJK for this compact fixture), then manually
    // add Monospace so both roles reference one registry resource.
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=enter",
    "--key=up",
    "--key=up",
    "--key=up",
    "--key=up",
    "--key=up",
    "--key=enter",
    "--key=esc",
    // Apply the staged map. Discovery read metadata only; this reads the
    // payload once and reflows once despite two role references.
    "--key=down",
    "--key=enter",
    "--key=wait",
    // Reopen Fonts in the same session. This must reuse the cached catalog and
    // show the same installed file without another payload read.
    "--key=doc",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=enter",
    "--key=enter",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=down",
    "--key=wait",
    // Pin the responsive final detail list: app header, dimmed paper, panel,
    // and selected role/action row. These pixels are rebaselined with the
    // Font Manager fixture when its layout changes.
    "--expect=0,2,0x1969",
    "--expect=0,20,0xbdf7",
    "--expect=24,50,0x3c9a",
    "--expect=100,60,0xffdf",
    "--expect=30,80,0xffdf",
    "--expect=200,140,0x3c9a",
    "--expect=31,137,0xffff",
    "--expect=319,239,0xbdf7",
  );
}

if (!fontFixture && !mathFixture && !mathReviewFixture &&
    !oversizedFormulaFixture && !scrollSwipeFixture && !pageFixture &&
    !tocFixture && !browserCancelFixture && !themeFixture && !stateFixture &&
    !fontMenuFixture) {
  args.push(
    // Scroll mode leaves the right side of the header free of a page label.
    "--expect=169,7,0x1969",
    // Reviewed heading, paragraph, list, Greek, and Cyrillic coverage pins
    // the current wrapping layout without depending on clipped lower blocks.
    "--expect=23,25,0x3c9a",
    "--expect=5,95,0x29a8",
    "--expect=77,97,0x7c11",
    "--expect=120,98,0xdefb",
    "--expect=45,167,0x29a8",
    "--expect=268,167,0x29a8",
    "--expect=61,197,0x29a8",
  );
}

if (mathFixture && !captureOnly) {
  args.push(
    // Latin Modern inline radical overbar and its reserved endpoint pixel.
    "--expect=103,65,0xdf1c",
    "--expect=104,65,0x29a8",
    "--expect=111,65,0x29a8",
    "--expect=112,65,0xffff",
    // The display radical's anti-aliased glyph edge touches the solid
    // continuation rule, whose right endpoint remains deliberately extended.
    "--expect=61,101,0xf7de",
    "--expect=62,101,0x29a8",
    "--expect=115,101,0x29a8",
    "--expect=116,101,0xffff",
    // The centered fraction rule spans the full Latin Modern numerator width.
    "--expect=5,130,0x29a8",
    "--expect=117,130,0x29a8",
    "--expect=118,130,0xffff",
    // Solid and partial coverage on Latin Modern matrix/vector parentheses.
    "--expect=150,124,0xd6ba",
    "--expect=151,124,0x29a8",
    "--expect=191,124,0x29a8",
    "--expect=192,124,0xce9a",
    "--expect=204,122,0x29a8",
    "--expect=242,122,0x29a8",
    // Both Latin Modern cases-brace stems remain solid.
    "--expect=284,117,0x29a8",
    "--expect=311,117,0x29a8",
    // These are the brace's own anti-aliased edge pixels; the former solid
    // blue two-pixel overflow cursor must not replace them.
    "--expect=313,132,0x7c31",
    "--expect=314,132,0x5b2d",
  );
}

if (mathReviewFixture && !captureOnly) {
  args.push(
    // The unary minus, plus, and subtraction strokes on the first row are
    // each eleven pixels wide in Latin Modern Math. Matching left/right edge
    // coverage pins their equal advances without depending on solid centers.
    "--expect=123,77,0xf7de",
    "--expect=133,77,0xa535",
    "--expect=149,77,0xf7de",
    "--expect=159,77,0xa535",
    "--expect=195,77,0xf7de",
    "--expect=205,77,0xa535",
    // The radical overbar keeps its anti-aliased leading edge and solid right
    // endpoint; both fraction rules have solid endpoints followed by white.
    "--expect=178,60,0xf7de",
    "--expect=233,60,0x29a8",
    "--expect=234,60,0xffff",
    "--expect=121,89,0x29a8",
    "--expect=234,89,0x29a8",
    "--expect=235,89,0xffff",
    "--expect=121,141,0x29a8",
    "--expect=234,141,0x29a8",
    "--expect=235,141,0xffff",
  );
}

if (pageFixture && !captureOnly) {
  args.push(
    // One forward screen step produces partial continuous reading progress.
    "--expect=0,0,0x3d4d",
    "--expect=319,0,0xbe39",
    // Horizontal Scroll has no synthetic page label in the upper-right header.
    "--expect=293,8,0x1969",
    "--expect=303,8,0x1969",
    "--expect=310,10,0x1969",
    "--expect=311,13,0x1969",
  );
}

if (tocFixture && !captureOnly) {
  args.push(
    // The post-Esc Down step moves Alpha's heading into the first paper row.
    // This pixel is white immediately after the menu closes and heading blue
    // only after that later navigation input produces its new frame.
    "--expect=8,18,0x3c9a",
  );
}

if (process.env.FIREBIRD_CONFIG) {
  args.push(`--config=${resolve(process.env.FIREBIRD_CONFIG)}`);
}

const result = spawnSync(process.env.BUN ?? "bun", args, {
  cwd: pocketRoot,
  encoding: "utf8",
  maxBuffer: 8 * 1024 * 1024,
  env: process.env,
});
process.stdout.write(result.stdout ?? "");
process.stderr.write(result.stderr ?? "");
if (result.error) {
  console.error(result.error.message);
  process.exit(2);
}
if (result.status !== 0) {
  // PocketJS revision 556ca6740ce1 can emit a complete schema-11 pass report
  // without the newly checked sameSessionTargetMode field, then reject that
  // report in its TypeScript wrapper. Accept only that exact non-relaunch
  // compatibility case; every transfer, stability, and framebuffer assertion
  // must still have passed.
  let compatibleFontMenuPass = false;
  const resultPath = resolve(outdir, "result.json");
  const combinedOutput = `${result.stdout ?? ""}\n${result.stderr ?? ""}`;
  if (fontMenuFixture && existsSync(resultPath) &&
      combinedOutput.includes(
        "Firebird pass report failed its transfer/framebuffer invariants",
      )) {
    try {
      const report = JSON.parse(readFileSync(resultPath, "utf8"));
      compatibleFontMenuPass =
        report.schemaVersion === 11 && report.status === "pass" &&
        report.sameSessionRelaunch === false &&
        report.sameSessionTargetMode === undefined &&
        report.transferProgress === 100 &&
        report.expectedTransferFiles === 1 &&
        report.completedTransferFiles === 1 &&
        report.mismatchedPixels === 0 &&
        report.stableMatches >= report.requiredStableMatches &&
        Array.isArray(report.expectations) &&
        report.expectations.length > 0 &&
        report.expectations.every((expectation) => expectation.matches === true);
    } catch {
      compatibleFontMenuPass = false;
    }
  }
  if (!compatibleFontMenuPass) process.exit(result.status ?? 2);
  console.log(
    "PocketJS compatibility: accepted verified schema-11 pass with omitted " +
    "sameSessionTargetMode (non-relaunch fixture).",
  );
}

const requiredMarkers = [
  "NMARKDOWN_IT/1 ENTER_MAIN",
  "NMARKDOWN_IT/1 FIXTURE_READY",
  ...(oversizedFormulaFixture
    ? ["NMARKDOWN_IT/1 OVERSIZED_FORMULA_READY"]
    : []),
  ...(scrollSwipeFixture ? ["NMARKDOWN_IT/1 SCROLL_SWIPE_READY"] : []),
  ...(fontMenuFixture ? ["NMARKDOWN_IT/1 FONT_MENU_FILE_READY"] : []),
  "NMARKDOWN_IT/1 HARFBUZZ_READY",
  ...(fontFixture ? ["NMARKDOWN_IT/1 FONT_READY"] : []),
  ...(browserCancelFixture ? [] : [
    "NMARKDOWN_IT/1 DOCUMENT_READ",
    "NMARKDOWN_IT/1 DOCUMENT_PARSED",
    "NMARKDOWN_IT/1 DOCUMENT_READY",
  ]),
  "NMARKDOWN_IT/1 RENDER_DONE",
  "NMARKDOWN_IT/1 PRESENTED",
  ...(fontMenuFixture ? [
    "NMARKDOWN_IT/1 FONT_CATALOG_SCANNED",
    "NMARKDOWN_IT/1 FONT_MANAGER_READY",
    "NMARKDOWN_IT/1 FONT_DETAIL_READY",
    "NMARKDOWN_IT/1 FONT_SUGGESTIONS_USED",
    "NMARKDOWN_IT/1 FONT_ROLE_TOGGLED",
    "NMARKDOWN_IT/1 FONT_DETAIL_BACK",
    "NMARKDOWN_IT/1 FONT_REGISTRY_READY resources=1",
    "NMARKDOWN_IT/1 FONT_CATALOG_CACHE_HIT",
    "NMARKDOWN_IT/1 FONT_MANAGER_READY",
    "NMARKDOWN_IT/1 FONT_DETAIL_READY",
  ] : []),
  ...(stateFixture ? ["NMARKDOWN_IT/1 STATE_SAVE_OK"] : []),
  ...(browserCancelFixture || stateFixture ? ["NMARKDOWN_IT/1 EXIT_OK"] : []),
];
let markerOffset = 0;
// PocketJS host progress and guest serial bytes share a stream. A host line
// can occasionally be inserted in the middle of a guest marker; removing the
// complete host line rejoins the two guest fragments before ordered-marker
// validation. Framebuffer/result checks above remain the source of pass/fail.
const serialTrace = `${result.stdout ?? ""}\n${result.stderr ?? ""}`
  .replace(/PocketJS harness:[^\r\n]*(?:\r?\n|$)/g, "")
  .replaceAll("\0", "");
for (const marker of requiredMarkers) {
  const next = serialTrace.indexOf(marker, markerOffset);
  if (next < 0) {
    console.error(`Firebird serial trace is missing required marker: ${marker}`);
    process.exit(1);
  }
  markerOffset = next + marker.length;
}
if (pageFixture) {
  const screenSteps = [...serialTrace.matchAll(
    /NMARKDOWN_IT\/1 PAGE_MOVE from=\d+\/\d+ target=\d+ scroll=(\d+) now=\d+\/\d+/g,
  )].map((match) => Number(match[1]));
  if (screenSteps.length < 3 || screenSteps[0] <= 0 || screenSteps[1] !== 0 ||
      screenSteps[2] !== screenSteps[0]) {
    console.error(
      `Horizontal screen-step trace did not preserve forward/back/forward: ${JSON.stringify(screenSteps)}`,
    );
    process.exit(1);
  }
}
if (oversizedFormulaFixture) {
  // Each probe marker is emitted inside the Ndless input call, before the
  // corresponding Viewer event returns. Requiring a fresh presented frame
  // after every marker proves focus, fine pan, swipe pan, and a
  // subsequent liveness input were all consumed in one app session.
  const interactionMilestones = [
    "NMARKDOWN_IT/1 OVERSIZED_FORMULA_ENTER_INPUT",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 OVERSIZED_FORMULA_RIGHT_INPUT",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 OVERSIZED_FORMULA_SWIPE_LEFT_INPUT",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 OVERSIZED_FORMULA_POST_SWIPE_LEFT_INPUT",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 OVERSIZED_FORMULA_FINAL_RIGHT_INPUT",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
  ];
  let interactionOffset = 0;
  for (const marker of interactionMilestones) {
    const next = serialTrace.indexOf(marker, interactionOffset);
    if (next < 0) {
      console.error(
        `Oversized-formula interaction trace is missing milestone: ${marker}`,
      );
      process.exit(1);
    }
    interactionOffset = next + marker.length;
  }

  const panRecords = [...serialTrace.matchAll(
    /NMARKDOWN_IT\/1 FORMULA_PAN event=([a-z-]+) before=(\d+) after=(\d+) max=(\d+)/g,
  )].map((match) => ({
    event: match[1],
    before: Number(match[2]),
    after: Number(match[3]),
    maximum: Number(match[4]),
  }));
  const finalPan = panRecords.at(-1);
  if (panRecords.length < 4 || finalPan == null ||
      finalPan.event !== "pan-right" ||
      finalPan.after !== finalPan.maximum ||
      finalPan.before >= finalPan.after) {
    console.error(
      `Oversized-formula capture did not finish at its right edge: ` +
      `${JSON.stringify(panRecords)}.`,
    );
    process.exit(1);
  }
}
if (scrollSwipeFixture) {
  const transitions = [
    {
      marker: "NMARKDOWN_IT/1 SCROLL_SWIPE_BASE_DOWN_INPUT",
      event: "line-down",
      direction: 1,
    },
    {
      marker: "NMARKDOWN_IT/1 SCROLL_SWIPE_LEFT_INPUT",
      event: "swipe-left",
      direction: -1,
    },
    {
      marker: "NMARKDOWN_IT/1 SCROLL_SWIPE_RIGHT_INPUT",
      event: "swipe-right",
      direction: 1,
    },
    {
      marker: "NMARKDOWN_IT/1 SCROLL_SWIPE_POST_SWIPE_DOWN_INPUT",
      event: "line-down",
      direction: 1,
    },
    {
      marker: "NMARKDOWN_IT/1 SCROLL_SWIPE_VERTICAL_UP_INPUT",
      event: "pointer",
      direction: 1,
    },
    {
      marker: "NMARKDOWN_IT/1 SCROLL_SWIPE_VERTICAL_DOWN_INPUT",
      event: "pointer",
      direction: -1,
    },
  ];
  let transitionOffset = 0;
  let previousAfter = null;
  for (let index = 0; index < transitions.length; ++index) {
    const transition = transitions[index];
    const inputAt = serialTrace.indexOf(transition.marker, transitionOffset);
    if (inputAt < 0) {
      console.error(
        `Scroll-swipe trace is missing physical input marker: ${transition.marker}`,
      );
      process.exit(1);
    }

    const positionPattern =
      /NMARKDOWN_IT\/1 SCROLL_POSITION event=([a-z-]+) before=(-?\d+) after=(-?\d+)/g;
    positionPattern.lastIndex = inputAt + transition.marker.length;
    const position = positionPattern.exec(serialTrace);
    if (!position) {
      console.error(
        `Scroll-swipe input had no subsequent position record: ${transition.marker}`,
      );
      process.exit(1);
    }
    const event = position[1];
    const before = Number(position[2]);
    const after = Number(position[3]);
    if (event !== transition.event) {
      console.error(
        `Scroll-swipe input produced ${event}; expected ${transition.event}.`,
      );
      process.exit(1);
    }
    if (previousAfter !== null && before !== previousAfter) {
      console.error(
        `Scroll-swipe continuity failed: previous=${previousAfter}, before=${before}.`,
      );
      process.exit(1);
    }
    if ((transition.direction < 0 && after >= before) ||
        (transition.direction > 0 && after <= before)) {
      console.error(
        `Scroll-swipe direction failed for ${event}: before=${before}, after=${after}.`,
      );
      process.exit(1);
    }
    if (index === 0 && (before !== 0 || after <= 0)) {
      console.error(
        `Scroll-swipe fixture did not establish a nonzero start: ` +
        `before=${before}, after=${after}.`,
      );
      process.exit(1);
    }

    let presentedOffset = position.index + position[0].length;
    for (const milestone of [
      "NMARKDOWN_IT/1 RENDER_START",
      "NMARKDOWN_IT/1 RENDER_DONE",
      "NMARKDOWN_IT/1 PRESENTED",
    ]) {
      const milestoneAt = serialTrace.indexOf(milestone, presentedOffset);
      if (milestoneAt < 0) {
        console.error(
          `Scroll-swipe transition ${event} is missing milestone: ${milestone}`,
        );
        process.exit(1);
      }
      presentedOffset = milestoneAt + milestone.length;
    }
    previousAfter = after;
    transitionOffset = presentedOffset;
  }
}
if (tocFixture) {
  const postEscapeMilestones = [
    "NMARKDOWN_IT/1 TOC_POST_ESC_DOWN_INPUT",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
  ];
  let postEscapeOffset = 0;
  for (const marker of postEscapeMilestones) {
    const next = serialTrace.indexOf(marker, postEscapeOffset);
    if (next < 0) {
      console.error(
        `TOC post-Esc liveness trace is missing required marker: ${marker}`,
      );
      process.exit(1);
    }
    postEscapeOffset = next + marker.length;
  }

  const presentedCount = serialTrace.match(/NMARKDOWN_IT\/1 PRESENTED/g)?.length ?? 0;
  if (presentedCount < 13) {
    console.error(
      `TOC jump trace presented only ${presentedCount} frames; expected at least 13.`,
    );
    process.exit(1);
  }
}
if (themeFixture) {
  const themeMarkers = [
    "NMARKDOWN_IT/1 THEME_DARK",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 THEME_MODAL_SWIPE_UP_INPUT",
    "NMARKDOWN_IT/1 THEME_MODAL_CLICK_INPUT",
    "NMARKDOWN_IT/1 THEME_MODAL_CLICK_RELEASE",
    "NMARKDOWN_IT/1 THEME_MODAL_SELECTION row=1",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
    "NMARKDOWN_IT/1 THEME_MODAL_BODY_SIZE size=16",
    "NMARKDOWN_IT/1 RENDER_START",
    "NMARKDOWN_IT/1 RENDER_DONE",
    "NMARKDOWN_IT/1 PRESENTED",
  ];
  let themeOffset = 0;
  for (const marker of themeMarkers) {
    const next = serialTrace.indexOf(marker, themeOffset);
    if (next < 0) {
      console.error(`Theme transition trace is missing required marker: ${marker}`);
      process.exit(1);
    }
    themeOffset = next + marker.length;
  }
  const touchStart = serialTrace.indexOf(
    "NMARKDOWN_IT/1 THEME_MODAL_SWIPE_UP_INPUT",
  );
  const touchEnd = serialTrace.indexOf(
    "NMARKDOWN_IT/1 THEME_MODAL_CLICK_RELEASE",
    touchStart,
  );
  const touchWindow = serialTrace.slice(touchStart, touchEnd);
  if (touchWindow.includes("THEME_MODAL_SELECTION") ||
      touchWindow.includes("THEME_MODAL_BODY_SIZE")) {
    console.error("Settings changed during an ignored swipe/touch activation.");
    process.exit(1);
  }
  const presentedCount = serialTrace.match(/NMARKDOWN_IT\/1 PRESENTED/g)?.length ?? 0;
  if (presentedCount < 8) {
    console.error(
      `Theme/touch trace presented only ${presentedCount} frames; expected at least 8.`,
    );
    process.exit(1);
  }
}
if (stateFixture) {
  const savedCount = serialTrace.match(/NMARKDOWN_IT\/1 STATE_SAVE_OK/g)?.length ?? 0;
  if (savedCount < 2) {
    console.error(
      `State fixture completed only ${savedCount} verified writes; expected at least 2.`,
    );
    process.exit(1);
  }
  if (serialTrace.includes("NMARKDOWN_IT/1 STATE_SAVE_FAIL")) {
    console.error("State fixture reported a failed state write.");
    process.exit(1);
  }
}
if (fontMenuFixture) {
  const detailCount =
    serialTrace.match(/NMARKDOWN_IT\/1 FONT_DETAIL_READY/g)?.length ?? 0;
  const cacheHitCount =
    serialTrace.match(/NMARKDOWN_IT\/1 FONT_CATALOG_CACHE_HIT/g)?.length ?? 0;
  const registry = serialTrace.match(
    /NMARKDOWN_IT\/1 FONT_REGISTRY_READY resources=(\d+) mono=(\d+) cjk=(\d+)/,
  );
  const presentedCount =
    serialTrace.match(/NMARKDOWN_IT\/1 PRESENTED/g)?.length ?? 0;
  const sharedResource = registry && registry[1] === "1" &&
    registry[2] !== "0" && registry[2] === registry[3];
  if (detailCount < 2 || cacheHitCount < 1 || !sharedResource ||
      presentedCount < 20) {
    console.error(
      `Font-manager trace was incomplete: details=${detailCount}, ` +
      `cacheHits=${cacheHitCount}, shared=${Boolean(sharedResource)}, ` +
      `frames=${presentedCount}.`,
    );
    process.exit(1);
  }
  if (serialTrace.includes("NMARKDOWN_IT/1 FONT_MENU_FILE_FAIL")) {
    console.error("Font-menu fixture could not create its CJK font file.");
    process.exit(1);
  }
}
console.log("nMarkdown serial milestones: PASS (raw file parsed, rendered, and presented).\n");
