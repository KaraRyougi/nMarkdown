# Mathematical LaTeX support

nMarkdown implements a bounded mathematical-layout language for Markdown
formulas. It is deliberately not a complete TeX interpreter: formula input is
parsed into a small box tree, laid out from OpenType MATH data, and rasterized
through the embedded Latin Modern Math face. This keeps memory use and failure
behavior predictable on a TI-Nspire CX while still covering a broad
LaTeX/AMS-style reading vocabulary.

Inline Markdown math starts in Text style and display math starts in Display
style. An invalid or unsupported formula produces a local error box; it does
not abort the document or leave the reader unresponsive.

## 1. Direct native symbol catalog

The exhaustive no-argument command table is
[`src/math/math_symbol_table.inc`](../src/math/math_symbol_table.inc). At this
revision it contains **514 command entries**, sorted for binary lookup. The
table was derived from KaTeX's mature `src/symbols.ts` registry at commit
`2c6143a6dd7c168cef602c1e29f8add66f7fcc19`, with operator limit behavior
checked against KaTeX's `src/functions/op.ts`, then filtered to values that can be
rendered by the embedded Latin Modern Math face, and supplemented with the
documented nMarkdown compatibility aliases. The provenance and license are in
[`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).

The count is a command-entry count, not a unique-glyph count. Several commands
are aliases for the same value, and named operators such as `\sin` render more
than one Latin Modern glyph.

| TeX atom class | Entries | What the class controls | Examples |
|---|---:|---|---|
| Ordinary | 116 | Variables, Greek, letterlike and miscellaneous symbols | `\alpha`, `\varepsilon`, `\N`, `\emptyset`, `\partial`, `\infty` |
| Operator | 72 | Named and large operators | `\sin`, `\lim`, `\sum`, `\int`, `\bigcup` |
| Binary | 67 | Spacing on both sides when used between operands | `\pm`, `\cdot`, `\cup`, `\land`, `\otimes` |
| Relation | 223 | Relation spacing | `\leq`, `\in`, `\subseteq`, `\rightarrow`, `\models` |
| Opening | 12 | Opening-delimiter spacing | `\langle`, `\lbrace`, `\lVert`, `\llbracket` |
| Closing | 12 | Closing-delimiter spacing | `\rangle`, `\rbrace`, `\rVert`, `\rrbracket` |
| Punctuation | 2 | Mathematical punctuation spacing | `\cdotp`, `\ldotp` |
| Inner | 10 | Compound/inner-expression spacing | `\dots`, `\cdots`, `\ddots`, `\ldots` |

Every table entry is exercised by data-driven parser and layout regressions.
Those checks require the exact UTF-8 value, atom class, large/limits flags, no
parser recovery, Latin Modern face selection, a nonzero glyph ID, and no
`.notdef` substitution. The source table remains the authoritative complete
list; the examples above are only category guides.

### Requested commands and compatibility aliases

| Input | Result and behavior |
|---|---|
| `\dot{x}` or `\dot x` | Dot-above accent. Every accent accepts either one unbraced atom or a braced group. |
| `A\cup B` | Native U+222A UNION, classified as Binary. |
| `x\rightarrow y` | Native U+2192 RIGHTWARDS ARROW, classified as Relation. `\to` is an alias. |
| `\N` | Native U+2115 DOUBLE-STRUCK CAPITAL N. `\Z`, `\Q`, `\R`, and `\C` are provided as matching shortcuts. These one-letter names are compatibility extensions; `\mathbb{N}` remains the portable LaTeX spelling. |
| `\lang x\rang` | Aliases for `\langle x\rangle`; they also work in `\left\lang ... \right\rang`. |
| `\empty` | Alias for `\emptyset` and `\varnothing`, all producing U+2205 EMPTY SET. |
| `p\or q` | Alias for `\lor` and `\vee`, all producing U+2228 LOGICAL OR as a Binary atom. `\and`, `\land`, and `\wedge` form the symmetric AND family. |
| `\text{A\textvisiblespace B}` | This is LaTeX's standard visible-space text command. nMarkdown renders it as U+2423 OPEN BOX (`␣`), and the control-word delimiter is not rendered. Direct `\textvisiblespace` inside a formula is also accepted as a compatibility convenience. This is a visible glyph, unlike the invisible width from `\space`. |

`\imath` and `\jmath` intentionally retain nMarkdown's product-specific
behavior: they use Latin Modern's dotted bold-italic i and j, including below
accents. They are not TeX's traditional dotless letter commands.

## 2. Structures and spacing

The parser supports:

- `\frac{a}{b}` and `\cfrac{a}{b}`;
- `\sqrt{x}` and indexed `\sqrt[n]{x}`;
- braced or single-atom superscripts and subscripts, in either order;
- `\left ... \right` delimiter groups, including `\left.` or `\right.` for an
  invisible side;
- `\text{...}`, `\operatorname{...}`, and `\tag{...}`;
- `\space`, `\quad`, `\qquad`, and the control-symbol spaces `\ `, `\,`,
  `\:`, `\;`, and `\!`;
- top-level `&` alignment cells and `\\` row breaks;
- formula-local zero-argument `\newcommand`, `\renewcommand`, and `\def`
  aliases declared at the start of a formula.

TeX's eight-class atom spacing table determines inter-atom spacing, using its
standard 3 mu thin, 4 mu medium, and 5 mu thick gaps. Binary operators are
demoted to Ordinary atoms in unary or operand-less positions, so `a-b` keeps
binary spacing while `-b`, `a=-b`, `(-b)`, and a trailing `a+` do not gain
spurious gaps. This is why a Binary
operator such as `\cup`, a Relation such as `\rightarrow`, and an Ordinary
symbol do not receive the same gaps. A literal ASCII hyphen in math is also
normalized to U+2212 MINUS SIGN so its advance and stroke match the plus sign.

Latin text in `\text{...}`, operator names, and equation tags stays in Latin
Modern Math instead of switching to the document Body face. A CJK cluster in
`\text{...}` may use the selected CJK fallback when Latin Modern has no glyph.

Macro aliases are intentionally bounded: at most 16 definitions, 32 letters
in a name, 256 replacement bytes per definition, eight expansion levels, and
16 KiB after expansion. Parameters and recursion are rejected; definitions do
not persist to another formula.

## 3. Accents and decorations

The following commands are layout operations, so they are not included in the
514-entry direct-symbol count.

| Family | Supported commands | Rendering behavior |
|---|---|---|
| Positioned accents | `\hat`, `\acute`, `\grave`, `\breve`, `\check`, `\tilde`, `\mathring`, `\vec`, `\dot`, `\ddot` | A simple one-glyph base is shaped with the appropriate combining mark. A wider or compound base uses a centered native fallback mark. |
| Horizontal rules | `\bar`, `\overline`, `\underline` | Width-aware one-pixel-or-thicker rules. |
| Arrows over/under | `\overleftarrow`, `\overrightarrow`, `\overleftrightarrow`, `\underleftarrow`, `\underrightarrow`, `\underleftrightarrow` | Width-aware rule and arrowhead primitives. |
| Braces over/under | `\overbrace`, `\underbrace` | Width-aware brace primitives; normal scripts can attach to the resulting box. |

Both `\dot x` and `\dot{x}` are valid and do not raise a recovery warning.
The same braced-or-one-atom rule applies to every accent in this table.

## 4. Math variants and styles

Braced variants are:

`\mathnormal`, `\mathrm`, `\mathbf`, `\bm`, `\bold`, `\mathit`, `\mathbb`,
`\mathcal`, `\mathscr`, `\mathsf`, `\mathtt`, and `\mathfrak`.

Declaration-style compatibility forms consume one following group or atom:

`\rm`, `\cal`, `\it`, `\Bbb`, `\bf`, `\mit`, `\sf`, `\scr`, `\tt`,
`\frak`, and `\boldsymbol`.

`\mathbf` and `\bold` select upright bold letters. `\bm` and
`\boldsymbol` preserve the normal italic variable style while adding bold
weight; digits remain upright bold because Unicode has no bold-italic digit
alphabet.

Supported letters and digits map to the matching mathematical Unicode forms
in Latin Modern Math; variants do not select the document Body or Monospace
role. Lowercase calligraphic input is supported where Latin Modern provides
the mathematical glyph.

The layout engine has Display, Text, Script, and ScriptScript styles.
`\displaystyle`, `\textstyle`, `\scriptstyle`, and
`\scriptscriptstyle` change the active style for the remainder of the current
row. Fractions and scripts automatically step down to a smaller style.

## 5. Delimiters and generic negation

Ordinary characters `()`, `[]`, braces, and vertical bars are accepted.
Named pairs include:

- `\lang`/`\rang` and `\langle`/`\rangle`;
- `\lparen`/`\rparen`, `\lbrack`/`\rbrack`, and
  `\lbrace`/`\rbrace`;
- `\lfloor`/`\rfloor` and `\lceil`/`\rceil`;
- `\lvert`/`\rvert`, `\lVert`/`\rVert`, and `\Vert` or control symbol
  `\|` for a double vertical bar;
- `\lgroup`/`\rgroup`, `\lBrack`/`\rBrack`, and
  `\llbracket`/`\rrbracket`.

After `\left` or `\right`, the compatibility aliases are normalized to the
renderer delimiter values. The layout selects Latin Modern OpenType MATH
variants when available and uses the calculator-oriented thin-stroke fallback
for supported tall delimiters.

`\not` accepts a following one-symbol operand, braced or unbraced. It appends
U+0338 COMBINING LONG SOLIDUS OVERLAY and lets HarfBuzz select a precomposed
negated relation when available or position the zero-advance overlay. Examples
include `\not=`, `\not\in`, and `\not\rightarrow`. A complex multi-node
operand is preserved rather than falsely replaced by a guessed negated glyph;
use a named negated relation from the direct catalog where one exists.

## 6. Large glyphs and movable limits

Glyph size and limits placement are separate properties.

- **Large glyph** chooses an OpenType MATH display variant, or a bounded scale
  fallback, only in Display style.
- **Movable limits** puts a superscript above and a subscript below the base in
  Display style. In Text, Script, and ScriptScript styles they remain at the
  side.

The catalog contains 24 large-glyph entries and 29 movable-limits entries:

| Flags | Commands |
|---|---|
| Large glyph + movable limits | `\sum`, `\prod`, `\coprod`, `\bigcap`, `\bigcup`, `\bigsqcap`, `\bigsqcup`, `\biguplus`, `\bigvee`, `\bigwedge`, `\bigodot`, `\bigoplus`, `\bigotimes`, `\intop` |
| Large glyph, side scripts | `\int`, `\iint`, `\iiint`, `\iiiint`, `\oint`, `\oiint`, `\oiiint`, `\intclockwise`, `\ointctrclockwise`, `\varointclockwise` |
| Normal glyph + movable limits | `\Pr`, `\argmax`, `\argmin`, `\det`, `\gcd`, `\inf`, `\injlim`, `\lim`, `\liminf`, `\limsup`, `\max`, `\min`, `\projlim`, `\smallint`, `\sup` |

This distinction keeps display integrals large without incorrectly moving
their limits above and below. Explicit `\limits` and `\nolimits` overrides are
not implemented.

## 7. Arrays, matrices, and alignment

Supported environments are:

`matrix`, `pmatrix`, `bmatrix`, `Bmatrix`, `vmatrix`, `Vmatrix`, `cases`,
`array`, `aligned`, `align`, and `align*`.

Arrays accept `l`, `c`, `r`, and `|` in the column preamble and `\hline` at row
boundaries. Matrices are centered by column; cases are left-aligned. Aligned
environments alternate right- and left-aligned cells around `&` markers.

In `align` and `aligned`, the first `\tag` on a row is extracted into a
separate right-side tag lane, so unequal tag widths do not move the equation
columns. The lane never changes cell alignment: all cells in the same
right-aligned column share one right edge, including untagged rows, while tags
end independently at the formula's outer edge.

Wide display formulas are not scaled down into unreadable text. The reader
keeps the native formula metrics and lets the user open the formula and pan it
horizontally.

## 8. Latin Modern boundary and intentional exclusions

The upstream audit shaped 396 unique Unicode codepoints from KaTeX's math
symbol registry through `assets/fonts/LatinModernMath.otf`; 29 codepoints
returned `.notdef`. nMarkdown does not silently substitute an unrelated
character merely to make an unsupported command appear successful.

| Missing Latin Modern codepoint(s) | Affected commands | Decision |
|---|---|---|
| U+02C9, U+02CA, U+02CB | Accent marks for `\bar`, `\acute`, `\grave` | Use a width-aware rule/U+00AF, U+00B4, and U+0060 respectively. |
| U+03DD | `\digamma` | Excluded; no different Greek letter is substituted. |
| U+2132, U+2141 | `\Finv`, `\Game` | Excluded. |
| U+21E0, U+21E2 | `\dashleftarrow`, `\dashrightarrow` | Excluded until synthesized/extensible arrows exist. |
| U+22D4 | `\pitchfork` | Excluded. |
| U+23B0, U+23B1 | `\lmoustache`, `\rmoustache` | Excluded until a native/stretchy form exists. |
| U+24C8 | `\circledS` | Excluded. |
| U+2571, U+2572 | `\diagup`, `\diagdown` | Excluded; slash substitutions would change the requested symbols. |
| U+25B9, U+25C3 | KaTeX forms of `\triangleright`, `\triangleleft` | Use native mathematical U+22B3 and U+22B2 forms. |
| U+2605 | `\bigstar` | Excluded. |
| U+29EB | `\blacklozenge` | Excluded. |
| U+2A5E | `\doublebarwedge` | Excluded. |
| U+2AB5–U+2ABA | `\precneqq`, `\succneqq`, `\precapprox`, `\succapprox`, `\precnapprox`, `\succnapprox` | Excluded; distinct relations are not collapsed. |
| U+2AC5, U+2AC6, U+2ACB, U+2ACC | `\subseteqq`, `\supseteqq`, `\subsetneqq`, `\supsetneqq` | Excluded; distinct relations are not collapsed. |

These exclusions are a font-and-layout boundary, not a promise that every
other KaTeX function is supported. KaTeX's function language is much larger
than its direct symbol registry.

## 9. Structural non-goals and deferred syntax

The following need real layout/grammar nodes and are not faked as Unicode
aliases:

- style-specific and ruleless fractions: `\tfrac`, `\dfrac`, `\binom`,
  `\tbinom`, and `\dbinom`;
- stacks and labeled extensibles: `\overset`, `\underset`, `\stackrel`,
  `\xleftarrow`, and `\xrightarrow`;
- explicit limit overrides: `\limits` and `\nolimits`;
- `\middle`, fixed-size `\big`/`\Big`/`\bigg`/`\Bigg` delimiter families,
  `\widehat`, `\widetilde`, `\widecheck`, `\overbracket`, and
  `\underbracket`;
- `alignat`, `alignedat`, `matrix*`, `rcases`, `cases*`, `subarray`,
  `multline`, `CD`, and automatic equation numbering;
- automatic line breaking inside one formula. Oversized formulas use explicit
  reader focus and panning instead.

The calculator reader also intentionally excludes browser/HTML commands,
images and URLs, formula-local colors and color boxes, package loading,
arbitrary `\char` access, counters, file input, global or parameterized TeX
programming, package-internal commands, and formula-local `Huge` through
`tiny` size changes. Those features either conflict with the reader's global
theme and pagination or require an unbounded TeX runtime.

## 10. Resource limits and recovery

| Limit | Value |
|---|---:|
| Formula source after macro expansion | 16 KiB |
| Lexer tokens | 8,192 |
| Parse/layout nesting | 64 levels |
| Generated math boxes | 16,384 |
| Matrix dimensions | 32 × 32 |

Unknown control sequences and malformed groups are reported locally. Hard
resource-limit failures stop only that formula. This bounded recovery model is
part of the supported interface; accepting a command name while drawing a
missing-glyph box is not considered support.
