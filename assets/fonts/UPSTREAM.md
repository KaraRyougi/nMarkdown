# Font provenance

The font binaries in this directory are pinned inputs. The runtime
`core.fpk` contains minimal printable-ASCII DejaVu Sans and DejaVu Sans Mono
bootstrap faces plus Latin Modern Math. Task checkboxes use fixed-size rasters ported from
GitHub's browser-native checkbox and do not add font glyphs. Full text faces
are external files assigned to runtime
roles on the calculator. License text lives in `THIRD_PARTY_NOTICES.md` and the
referenced files. The embedded DejaVu UI/Mono bootstraps omit non-runtime copyright,
license, URL, description, legacy, and localized `name` records. Latin Modern
Math retains its complete original metadata; the UI does not display font
metadata.

## DejaVu Sans bootstrap and optional text faces

- Runtime roles: embedded Body bootstrap for UI/simple ASCII text and embedded
  Monospace bootstrap for code.
- External files: optional DejaVu Sans for Body, DejaVu Sans Oblique for Body
  Italic, and DejaVu Sans Mono for Monospace.
- Release: DejaVu `2.35`.
- Upstream files: `DejaVuSans.ttf` (756,072 bytes),
  `DejaVuSans-Oblique.ttf` (633,840 bytes), and `DejaVuSansMono.ttf`
  (340,712 bytes).
- SHA-256, Body: `3fdf69cabf06049ea70a00b5919340e2ce1e6d02b0cc3c4b44fb6801bd1e0d22`.
- SHA-256, Body Oblique: `ccdf74b350f11fd3dd5774de50e5e6346a1a5da1f5b7d5fb83590665e97a5213`.
- SHA-256, Monospace: `b4a6c3e4faab8773f4ff761d56451646409f29abedd68f05d38c2df667d3c582`.
- License: Bitstream Vera / Arev-derived notice in `LICENSE_DEJAVU`.

The three upstream files are unchanged from the DejaVu distribution. The
embedded bootstraps are deterministic subsets of `DejaVuSans.ttf` and
`DejaVuSansMono.ttf`:

- Bootstrap: `DejaVuSans-UI.ttf`, 25,644 bytes; SHA-256
  `f7cd16c94a93af6fe66b3e2197980937a2dfc5629d97f9c338decdd882871fc6`.
- Monospace bootstrap: `DejaVuSansMono-UI.ttf`, 17,492 bytes; SHA-256
  `db1d6e4591baac0b5d9c69bea9416ab4c8e06993fb31068b5b80e1ac1166b3`.
- Coverage: `U+0020-007E,U+FFFD` from `DejaVuSans-UI.unicodes`; SHA-256
  `7112d84ce3882b0afecd4b5d5a23a657c75d4ecca9d160baa79fdbd15814e601`.

Run `make ui-font-subset` to regenerate both. The subsets keep only OpenType
`name` IDs 1, 2, 4, and 6 in English: family, style, full name, and PostScript
name. It omits copyright, license, URL, description, legacy, and other
localized name records because the embedded face is identified by the pack
manifest and attribution lives in `THIRD_PARTY_NOTICES.md`. The following
larger deterministic subsets retain the family/style identity, TrueType
outlines and hinting, and OpenType layout, but are external
calculator-transfer assets and are not embedded in `core.fpk`:

- Body: `DejaVuSans-CX.ttf`, 270,448 bytes; SHA-256
  `f2d339bed21b9c4500c568c7176d19c608fcac95d0f3522970f81f710a4a134e`.
- Body Oblique: `DejaVuSans-Oblique-CX.ttf`, 252,408 bytes; SHA-256
  `67652cb47038289d4fb432a42005be17ef142afd654a00331c22d3d2d05e7a14`.
- Monospace: `DejaVuSansMono-CX.ttf`, 175,164 bytes; SHA-256
  `c55e166e458bfba8bf89e5ef8f4e2ffe4fa40f30c5f11025b5e6c2999dde209e`.
- Shared Unicode-set SHA-256:
  `ddfe0ffc079ce5edddf959404ba473d3737ea52aaa0e35035539801d91587d76`.

Run `make dejavu-font-subsets` to regenerate all three optional device fonts
from the pinned upstream files and `DejaVu-CX.unicodes`. A regular-only
Body role without an italic companion still uses FreeType's outline oblique transform.
The pack manifest does not add a second standalone license payload. Direct
external fonts and the optional build-time external subsets retain the complete
source OpenType `name` table, including copyright, license, URL, and description
records. The embedded DejaVu UI/Mono bootstraps are the intentional exception noted
above.

## Sarasa Fixed SC Regular

- Role use: one external file can be assigned to Body, Monospace, and/or CJK.
  Multiple assignments share one loaded font resource.
- Upstream: [`be5invis/Sarasa-Gothic`](https://github.com/be5invis/Sarasa-Gothic).
- Release: `v1.0.40`.
- Archive: `SarasaFixedSC-TTF-1.0.40.7z`, 64,781,281 bytes.
- Archive SHA-256: `1d132e7a37ba4fb431322c6dc26d4a54ef34f4a86ee0b8a683fb04a28cd5014f`.
- Upstream `SarasaFixedSC-Regular.ttf`: 24,909,420 bytes.
- Upstream font SHA-256: `320afeda949aa047d189df29ec7e2caadf9a8eb7890c776b57828c4a2c0e87e6`.
- CX/CX II subset: `SarasaFixedSC-Regular-CX.ttf`, 6,105,504 bytes.
- Subset SHA-256: `e2a9016b9bfd543945f53e5e89867005febe85f4087035a4590ba2906e9726db`.
- Unicode-set SHA-256: `8ac67d84f8d1804274036226d5edf7246a2b62338cb3c9c151c13950a41839de`.
- License: SIL Open Font License 1.1 in `LICENSE_SARASA`.

The calculator subset retains the upstream **Sarasa Fixed SC / Regular**
family, style, and `Sarasa-Fixed-SC-Regular` PostScript identity. It includes
the GB2312 Simplified-Chinese core, the JIS/CP932 Japanese core, CJK punctuation
and radicals, kana, fullwidth forms, arrows, box drawing, and common symbols.
It contains 12,278 requested Unicode codepoints. At 6,105,504 bytes it fits the
original CX 6 MiB limit by 185,952 bytes and also fits the CX II 12 MiB limit.

The checked-in codepoint list was generated from Unicode's pinned CP936 and
CP932 mapping tables with `tools/fontpack/cjk_core_unicodes.mjs`:

```sh
node tools/fontpack/cjk_core_unicodes.mjs CP936.TXT CP932.TXT \
  assets/fonts/SarasaFixedSC-CX.unicodes
```

- CP936 table SHA-256: `b86f601c575e9ab457380b6f7abef03c75499cc6075bdc8b4b27f3f2de74bf6a`.
- CP932 table SHA-256: `c9bc0b0cd42e0fbcb82a09635bb5abed86afbdd4abc9e76fa5716638217cb59f`.

The font subset was generated with HarfBuzz 14.2.1. `make cjk-font-subset`
runs the equivalent command:

```sh
hb-subset assets/fonts/SarasaFixedSC-Regular.ttf \
  --output-file=assets/fonts/SarasaFixedSC-Regular-CX.ttf \
  --unicodes-file=assets/fonts/SarasaFixedSC-CX.unicodes \
  --name-IDs='*' --name-legacy --name-languages='*'
```

`make cjk-font` creates the calculator-transferable
`build/fonts/SarasaFixedSC-Regular.ttf.tns`. The full upstream TTF is retained
only as the reproducible source; its 24.9 MB size exceeds both on-device font
limits.

## Latin Modern Math

- Role: embedded Math face.
- Upstream file: `LatinModernMath.otf`, 733,736 bytes; SHA-256
  `6075562b771f8b82f0c179e363389684f2dd09de30038269e2628e504bd7be0f`.
- Runtime file: `LatinModernMath-Core.otf`, 717,640 bytes; SHA-256
  `9e357553fa053b7d87295f0a5237739f957b7afa88d788164e6f588945c43566`.
- License: GUST Font License in `LICENSE_LATIN_MODERN_MATH`.

`make core-math-font` preserves every glyph ID, outline, layout table, MATH
table, OpenType name record, and the original CFF Top DICT Notice. This includes
the source copyright, license, URL, and descriptive metadata. The pack manifest
does not append a second copy of the standalone license file, and the UI does
not display font metadata.

## STIX Two Math (unpackaged reference only)

`STIXTwoMath.otf` and `STIXTwoMath-Braces.otf` are retained as development
comparison inputs from the earlier math-renderer evaluation. No manifest,
generated source, native target, or transferable font package references
them; Latin Modern Math is the sole runtime Math face. Their SIL Open Font
License 1.1 is retained in `LICENSE_STIX_TWO`.
