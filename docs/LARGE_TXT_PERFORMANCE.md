# Large TXT paging benchmark

This benchmark targets the 2,622,979-byte UTF-8 Chinese novel supplied as
`~/Downloads/红楼梦.txt.tns`. TXT uses the production bounded-cache reader:
there is no Markdown IR, global line index, or exact page count.

Build and reproduce:

```sh
cmake --build build/desktop --target nmarkdown-large-txt-bench
build/desktop/nmarkdown-large-txt-bench \
  ~/Downloads/红楼梦.txt.tns \
  assets/fonts/SarasaFixedSC-Regular-CX.ttf \
  1
```

The final argument is the number of idle work polls between page turns. Times
below are desktop wall-clock measurements, not claims about calculator speed;
they separate the visible frame from work deliberately deferred until after
that frame has been presented.

The latest one-idle-poll run with the real compact Sarasa CJK font measured:

| Operation | Samples | Average | p95 | Maximum | Glyph misses |
|---|---:|---:|---:|---:|---:|
| Page Down visible render | 24 | 0.241 ms | 0.749 ms | 1.138 ms | 23 |
| Page Down post-present warm | 24 | 6.425 ms | — | 17.424 ms | — |
| Line Down visible render | 120 | 0.062 ms | 0.104 ms | 0.758 ms | 4 |
| Line Down post-present warm | 120 | 0.938 ms | — | 3.385 ms | — |

Opening the file itself took 7.971 ms on the desktop filesystem, loading the
font took 7.857 ms, setting the TXT source took 0.173 ms, and the cold first
visible render took 12.558 ms. Its first post-present warm step took
10.131 ms. The visible navigation frame is therefore no longer held for
speculative glyph rasterization.

The current path:

- paints only lines intersecting the 220 px viewport; painting no longer
  triggers layout of a completely invisible following screen;
- presents a loading card before large-file work, reads only the first 64 KiB
  for the first screen, and then extends a reserved 1 MiB sequential raw-byte
  cache in 32 KiB idle quanta;
- admits bounded runs at physical-line/whitespace boundaries where possible,
  shapes each run once with HarfBuzz, wraps the shaped output at cluster
  boundaries, and partitions the resulting lines into up to eleven screens. It
  no longer shapes fixed 256-byte fragments or reshapes overlapping prefixes
  at later screen boundaries;
- retains five previous and ten future shaped screens plus the small
  page-start-offset ring;
- presents the requested frame before warming the next line or page, and uses
  later idle slices to replenish the shaped-screen and raw-byte windows.

Glyph warming uses lazily allocated 256x256 A8 atlas pages with an 8 MiB
logical ceiling. If TI-OS refuses another bitmap page, the cache now reuses its
least-recently-used allocated page instead of repeatedly failing on an empty
logical page. It does not retain RGB565 page images. Three such 320x220 images
would cost about 413 KiB before bookkeeping and would require invalidation
after theme, font, spacing, search, and focus changes.

The targeted ARM/newlib Firebird run uses a self-contained 2,622,979-byte
UTF-8/CJK TXT fixture. The recorded streamed-source run reached document-ready
with 1,807,137 requested live heap bytes and a 1,811,769-byte lifetime peak;
the tested presentation reached 1,814,038 live and 1,814,074 peak, with zero
allocation failures and zero tracking overflows. Those figures include the
153,600-byte experimental direct scanout that has since been replaced by a
same-sized native-layout staging buffer on HW-W/CX II, so their requested-heap
impact remains representative. A single 2,622,980-byte full-file allocation
failed under the same guest, so reverting to a retained whole-book
`std::string` would make this exact novel less reliable rather than faster.

Firebird validates the ARM load path and allocator behavior, but it accepted a
direct heap-backed PL111 scanout that rendered as gibberish on physical
hardware. Production therefore keeps TI-OS's scanout address, prepares the
native HW-W orientation before Vsync, cleans that staging source, waits for a
fresh vertical-compare edge, and uses the supported contiguous Ndless
`lcd_blit`. The real Sarasa/HarfBuzz navigation timings above are from the host
benchmark; physical CX/CX II hardware remains the final display and
performance authority.
