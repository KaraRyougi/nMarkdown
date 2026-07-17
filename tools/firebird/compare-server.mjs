#!/usr/bin/env node

import {createServer} from "node:http";
import {existsSync} from "node:fs";
import {readFile} from "node:fs/promises";
import {dirname, extname, join, normalize, resolve} from "node:path";
import {fileURLToPath} from "node:url";

const toolRoot = dirname(fileURLToPath(import.meta.url));
const projectRoot = resolve(toolRoot, "../..");
const webRoot = resolve(toolRoot, "web");
const coreFont = resolve(projectRoot, "assets/fonts/DejaVuSans.ttf");

function argument(name, fallback) {
    const prefix = `--${name}=`;
    const value = process.argv.find((item) => item.startsWith(prefix));
    return value ? value.slice(prefix.length) : fallback;
}

const frame = resolve(argument("frame", resolve(projectRoot, "build/firebird/frame.ppm")));
const fontArgument = argument("font", process.env.FIREBIRD_FONT || "");
const font = fontArgument ? resolve(fontArgument) : "";
const port = Number(argument("port", "8091"));

if (!existsSync(frame) || !font || !existsSync(font)) {
    console.error(`Comparison inputs are missing.\nFrame: ${frame}\nSource font: ${font || "(pass --font=FONT.ttf or set FIREBIRD_FONT)"}`);
    process.exit(2);
}

const mime = {
    ".css": "text/css; charset=utf-8",
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".mjs": "text/javascript; charset=utf-8",
    ".ppm": "image/x-portable-pixmap",
    ".ttf": "font/ttf",
    ".otf": "font/otf",
};

function safePath(root, relative) {
    const clean = normalize(relative).replace(/^[/\\]+/, "");
    if (clean.startsWith("..")) throw new Error("invalid path");
    return join(root, clean);
}

createServer(async (request, response) => {
    try {
        const pathname = decodeURIComponent(new URL(request.url, "http://localhost").pathname);
        let path;
        if (pathname === "/frame.ppm") {
            path = frame;
        } else if (pathname === "/fixture.ttf") {
            path = font;
        } else if (pathname === "/core.ttf") {
            path = coreFont;
        } else {
            path = safePath(webRoot, pathname === "/" ? "index.html" : pathname.slice(1));
        }
        const data = await readFile(path);
        response.writeHead(200, {
            "Cache-Control": "no-store",
            "Content-Type": path === font || path === coreFont
                ? (mime[extname(path)] || "font/ttf")
                : (mime[extname(path)] || "application/octet-stream"),
        });
        response.end(data);
    } catch {
        response.writeHead(404, {"Content-Type": "text/plain; charset=utf-8"});
        response.end("Not found");
    }
}).listen(port, "127.0.0.1", () => {
    console.log(`nMarkdown Firebird/browser comparison: http://127.0.0.1:${port}`);
});
