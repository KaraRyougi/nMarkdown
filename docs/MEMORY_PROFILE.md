# Memory profiling external fonts

The font size limits and heap use are deliberately separate measurements.
External TTF/OTF files opened through the calculator's Stdio filesystem are
now random-access streams. Their complete on-disk byte count is therefore not
their steady-state heap cost.

## Streamed-font baseline measurement

The reported failing file was measured directly:

- file: `fusion-pixel-12px-proportional-zh_hans.ttf.tns`
- SHA-256: `5b27e9eb9d9dd93cff727d8919ddd2e7a482b19314b62991cb1e7806852e8734`
- file size: 7,012,032 bytes
- largest table: `glyf`, 6,259,594 bytes
- next largest tables: `post`, 304,077 bytes; `loca`, 144,860 bytes;
  `hmtx`, 142,094 bytes; `cmap`, 78,366 bytes

The production desktop renderer was cold-run against the same Markdown fixture
with and without that exact font. Across five cold processes, zsh's peak-RSS
counter had a 4,064 KiB baseline median and a 4,992 KiB Fusion Pixel median: a
928 KiB increase, not a 7 MiB retained copy. This measurement includes active
HarfBuzz table-provider shaping and rendered CJK glyphs. It is comparative host evidence,
not a substitute for the ARM/newlib allocator measurement below. This RSS
comparison predates the pinned-metrics stream mode described next.

### Storage-aware FreeType stream mode (2026-07-16)

The streamed external-font path now retains the small `hmtx` and `vmtx`
tables, capped at 256 KiB combined, and uses a 32 KiB LRU of aligned 1 KiB
blocks for the remaining font. The exact Fusion Pixel face therefore adds
219,110 bytes of pinned metrics plus 32,768 bytes of block storage (251,878
bytes, about 246 KiB, excluding small vector metadata). Its 6,259,594-byte
`glyf` table remains storage-backed.

Only streamed external faces use `FT_LOAD_NO_AUTOHINT`; native TrueType hints
remain enabled. This avoids FreeType's CJK auto-hinter scanning many unrelated
glyphs through the stream while leaving memory-backed core and math faces
unchanged. Idle warming examines up to 32 already-cached glyph keys in one
poll, but stops after one cold streamed glyph.

The host corpus benchmark now opens the font through the same custom
`FT_Stream`, sends actual forward `SwipeUp` gestures in horizontal-page mode,
counts backing font reads, and measures gesture-to-visible latency. With
`红楼梦.txt.tns` and the exact Fusion font:

- 30 immediate swipes, no injected delay: source byte 0 to 14,219; render
  maximum 0.066 ms; zero render-time glyph misses; at most two backing reads
  in one idle poll.
- 30 immediate swipes with a synthetic 1 ms penalty on every backing font
  read: gesture-to-visible average 47.083 ms, p95 92.517 ms, maximum
  114.306 ms; at most two backing reads in one idle poll.
- 30 swipes with a synthetic 5 ms read penalty and 128 idle scheduler polls
  between gestures: all target pages were already ready, with zero deferred
  polls or page-time font reads and a 0.616 ms maximum gesture-to-visible
  time.

The injected-delay runs verify that latency is bounded by real stream
transactions rather than host filesystem speed. They are not measurements of
TI hardware; a physical calculator remains authoritative.

A fresh CX II Firebird ARM run also completed 30 consecutive forward touch
swipes on the self-contained 2,622,979-byte UTF-8/CJK fixture, advancing from
page 1 to page 31 with a render and present after every gesture. Its final
framebuffer/liveness report passed with zero pixel mismatches. This is a
functional architecture check, not a NOR timing measurement.

Consequently, CX II now permits 20 MiB per external font and 20 MiB across
unique selected files. This remains an on-disk admission budget. Original CX
stays at 6 MiB per file and 8 MiB total until the same physical-device profile
has been recorded there.

### Exact CX II Firebird reproduction (2026-07-15)

The ARM/newlib profile was also run in a CX II Firebird guest with the exact
reported pair:

- `红楼梦.txt.tns`: 2,622,979 bytes, SHA-256
  `50b22882745a2d17d227e4359e97fe74d7b854705d031f3e08707f836e8c2ba2`
- Fusion Pixel: 7,012,032 bytes, SHA-256 shown above

The old whole-file TXT path failed before decoding or parsing while requesting
one 2,622,980-byte allocation. Dividing the same copy into retained 32 KiB,
8 KiB, and 2 KiB allocations only moved the failure later: aggregate free heap,
not the configured 20 MiB on-disk admission limit, was the constraint.

That indexed implementation has now been replaced. Valid UTF-8 TXT never
enters the Markdown IR: the Ndless reader fills only the first 64 KiB before
the first screen, then grows a reserved 1 MiB sequential source window in
32 KiB idle quanta. It retains a five-previous/ten-future shaped-screen window.
There is no global line index or exact page count. Memory-backed decoded
sources expose their existing storage directly and are not duplicated.
The current targeted CX II Firebird profile with the 2,622,979-byte UTF-8/CJK
fixture and built-in fallback rendering is:

```text
viewer_ready    current=607061   lifetime_peak=612157
document_read   current=688518   lifetime_peak=819595
document_parsed current=690211   lifetime_peak=819595
document_ready  current=1807137  lifetime_peak=1811769
presented       current=1814038  lifetime_peak=1814074
post_present    current=1814038  lifetime_peak=1814074
```

The measured requested-heap peak through first presentation was 1,814,074
bytes (about 1.73 MiB), with zero allocation failures and zero tracking
overflows. That historical profile included a 153,600-byte experimental direct
scanout. Production now uses a same-sized native-layout staging buffer on
HW-W/CX II, so the recorded requested-heap total remains representative of the
current two-buffer presentation allocation. This does not include
executable/static storage, stack, allocator metadata, or the OS-owned LCD
scanout, as described under Interpretation limits. The direct heap-backed
page-flip experiment was removed after producing corrupted output on physical
hardware. The current staging buffer is only a cache-cleaned source for the
supported Ndless `lcd_blit`; it is never installed as the PL111 scanout
address.

## Instrumented calculator build

Build the diagnostic-only package with:

```sh
export NDLESS_SDK=/path/to/Ndless/ndless-sdk
export PATH="$NDLESS_SDK/bin:$PATH"
make ndless-memory-profile
```

The result is:

```text
build/ndless-memory-profile/nmarkdown-memory-profile.tns
```

This build wraps `malloc`, `calloc`, `realloc`, and `free` and records requested
live bytes, lifetime peak bytes, allocation failures, and the peak since the
most recent font-role application began. It uses a fixed 8,192-slot allocation
table, so measurement itself never allocates. The profile build has about
68 KiB more static load memory than the corresponding production build; that
tracking table is not present in the normal `nmarkdown.tns`.

On a calculator:

1. Put the font and the memory-profile `.tns` anywhere under My Documents.
2. Launch the profile build with no external roles assigned. Open a small CJK
   test document, press Ctrl+D, and record the `Heap KiB` row.
3. Close Diagnostics. Open Reader Settings → Fonts, select the font, assign
   CJK, return to Installed fonts, and choose Apply changes.
4. Let the CJK page render, press Ctrl+D again, and record the same row.
5. Repeat while replacing an already assigned Sarasa face. This captures the
   temporary old/new registry overlap as well as the resulting steady state.

The row is formatted as:

```text
Heap KiB: now CURRENT / peak LIFETIME / font LAST_APPLY
```

When a serial console is attached, each role application also prints an exact
byte record:

```text
NMARKDOWN_MEMORY/1 font_apply=ok current=... lifetime_peak=...
font_peak=... allocations=... failures=... tracking_overflows=...
```

Any nonzero `tracking_overflows` value invalidates the byte totals and should
be reported. A nonzero failure count is evidence of allocator pressure even if
the previous font registry was restored successfully.

## Interpretation limits

The probe counts requested heap payload bytes. It does not include executable
and static data, allocator bookkeeping, free-block fragmentation, TI-OS memory,
or the LCD framebuffer. Firebird is useful for verifying that the ARM build and
counter path work, but physical CX/CX II results remain authoritative for
available heap and fragmentation. The normal reader also catches
`std::bad_alloc`, retains the previous font registry, and reports a visible
error instead of terminating.
