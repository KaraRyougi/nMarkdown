const byId = (id) => document.getElementById(id);
const firebirdCanvas = byId("firebird");
const browserCanvas = byId("browser");
const overlayCanvas = byId("overlay");
const blend = byId("blend");
const blendValue = byId("blend-value");
const status = byId("status");

function ppmToken(bytes, state) {
    while (state.offset < bytes.length) {
        const byte = bytes[state.offset];
        if (byte === 35) {
            while (state.offset < bytes.length && bytes[state.offset] !== 10) ++state.offset;
        } else if (byte <= 32) {
            ++state.offset;
        } else {
            break;
        }
    }
    const start = state.offset;
    while (state.offset < bytes.length && bytes[state.offset] > 32) ++state.offset;
    return new TextDecoder().decode(bytes.subarray(start, state.offset));
}

function parsePpm(buffer) {
    const bytes = new Uint8Array(buffer);
    const state = {offset: 0};
    if (ppmToken(bytes, state) !== "P6") throw new Error("Firebird frame is not P6 PPM.");
    const width = Number(ppmToken(bytes, state));
    const height = Number(ppmToken(bytes, state));
    const maximum = Number(ppmToken(bytes, state));
    if (width !== 320 || height !== 240 || maximum !== 255) {
        throw new Error(`Expected a 320 × 240, 8-bit Firebird frame; got ${width} × ${height}.`);
    }
    if (bytes[state.offset] === 13 && bytes[state.offset + 1] === 10) {
        state.offset += 2;
    } else if (bytes[state.offset] <= 32) {
        ++state.offset;
    }
    if (bytes.length - state.offset < width * height * 3) {
        throw new Error("Firebird frame is truncated.");
    }
    const image = new ImageData(width, height);
    for (let source = state.offset, target = 0; target < image.data.length;
         source += 3, target += 4) {
        image.data[target] = bytes[source];
        image.data[target + 1] = bytes[source + 1];
        image.data[target + 2] = bytes[source + 2];
        image.data[target + 3] = 255;
    }
    return image;
}

function isCjk(character) {
    const codepoint = character.codePointAt(0) ?? 0;
    return (codepoint >= 0x2e80 && codepoint <= 0x9fff) ||
           (codepoint >= 0x3040 && codepoint <= 0x30ff) ||
           (codepoint >= 0xac00 && codepoint <= 0xd7af) ||
           (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
           (codepoint >= 0xff00 && codepoint <= 0xffef) ||
           (codepoint >= 0x20000 && codepoint <= 0x323af);
}

function drawRoleText(context, text, x, baseline, size, role = "body") {
    let pen = x;
    let run = "";
    let cjk = false;
    const flush = () => {
        if (!run) return;
        const family = role === "mono" || cjk ? "NMarkdownFixture" : "NMarkdownCore";
        context.font = `${size}px ${family}`;
        context.fillText(run, pen, baseline);
        pen += context.measureText(run).width;
        run = "";
    };
    for (const character of text) {
        const nextCjk = role === "mono" || isCjk(character);
        if (run && cjk !== nextCjk) flush();
        cjk = nextCjk;
        run += character;
    }
    flush();
}

async function renderBrowserReference() {
    const [core, fixture] = await Promise.all([
        new FontFace("NMarkdownCore", "url(/core.ttf)").load(),
        new FontFace("NMarkdownFixture", "url(/fixture.ttf)").load(),
    ]);
    document.fonts.add(core);
    document.fonts.add(fixture);
    await document.fonts.ready;

    const context = browserCanvas.getContext("2d", {alpha: false});
    context.fillStyle = "#e8ebf1";
    context.fillRect(0, 0, 320, 240);
    context.fillStyle = "#fffffc";
    context.fillRect(0, 18, 320, 222);
    context.fillStyle = "#192d4a";
    context.fillRect(0, 0, 320, 18);
    context.fillStyle = "#bfc5cf";
    context.fillRect(8, 4, 84, 10);
    context.fillStyle = "#3bab6f";
    context.fillRect(8, 4, 42, 10);
    context.strokeStyle = "#788290";
    context.strokeRect(8.5, 4.5, 83, 9);
    context.fillStyle = "#fff";
    context.font = "8px NMarkdownCore";
    context.textAlign = "center";
    context.fillText("1 / 2", 50, 13);
    context.textAlign = "start";
    context.textBaseline = "alphabetic";
    context.fillStyle = "#fff";
    drawRoleText(context, "nMarkdown", 104, 14, 11);
    context.fillStyle = "#3d90d6";
    context.font = "bold 22px NMarkdownCore";
    context.fillText("Monospace + CJK on", 16, 42);
    context.fillText("CX II", 16, 65);
    context.fillStyle = "#2d3440";
    drawRoleText(context, "Body fallback:", 16, 99, 15);
    drawRoleText(context, "中文排版测试，日本語かなカナの表示。", 16, 122, 14);
    drawRoleText(context, "Inline code:", 16, 150, 15);
    context.fillStyle = "#eff2f6";
    context.fillRect(112, 134, 184, 22);
    context.fillStyle = "#2d3440";
    drawRoleText(context, "const width = 320;", 118, 150, 14, "mono");
    context.fillStyle = "#eff2f6";
    context.fillRect(12, 164, 296, 66);
    context.fillStyle = "#3d90d6";
    context.fillRect(12, 164, 3, 66);
    context.fillStyle = "#2d3440";
    drawRoleText(context, "grid[02] = 320x240;", 20, 184, 13, "mono");
    drawRoleText(context, "中文：标点换行，日本語かなカナ：句読点。",
                 20, 205, 13, "mono");
}

function drawOverlay() {
    const context = overlayCanvas.getContext("2d", {alpha: false});
    const browserAlpha = Number(blend.value) / 100;
    context.globalAlpha = 1;
    context.drawImage(firebirdCanvas, 0, 0);
    context.globalAlpha = browserAlpha;
    context.drawImage(browserCanvas, 0, 0);
    context.globalAlpha = 1;
    blendValue.value = `${blend.value}%`;
}

async function initialize() {
    try {
        const [frameResponse] = await Promise.all([
            fetch("/frame.ppm"),
            renderBrowserReference(),
        ]);
        if (!frameResponse.ok) throw new Error(`Firebird frame: HTTP ${frameResponse.status}`);
        const firebirdImage = parsePpm(await frameResponse.arrayBuffer());
        firebirdCanvas.getContext("2d", {alpha: false}).putImageData(firebirdImage, 0, 0);
        drawOverlay();
        status.textContent = "Ready — compare the mixed fallback line and code cells at native 320 × 240 resolution.";
    } catch (error) {
        status.textContent = error instanceof Error ? error.message : String(error);
        status.className = "error";
    }
}

blend.addEventListener("input", drawOverlay);
initialize();
