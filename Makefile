.PHONY: all desktop test generated-font-assets fontpack mathfont cjk-font \
	ui-font-subset core-math-font dejavu-font-subsets oblique-font-subset cjk-font-subset \
	entities unicode-tables ndless ndless-memory-profile \
	ndless-firebird-memory-profile \
	ndless-firebird-large-txt-profile \
	ndless-firebird-real-novel \
	ndless-firebird ndless-firebird-math ndless-firebird-math-review \
	ndless-firebird-imath \
	ndless-firebird-symbols \
	ndless-firebird-oversized-formula \
	ndless-firebird-scroll-swipe \
	ndless-firebird-keymap \
	ndless-firebird-page ndless-firebird-formats ndless-firebird-toc \
	ndless-firebird-esc-liveness \
	ndless-firebird-browser-cancel ndless-firebird-theme \
	ndless-firebird-state ndless-firebird-font-menu \
	firebird-test firebird-math-test \
	firebird-math-review-test firebird-imath-test \
	firebird-symbols-test \
	firebird-oversized-formula-test \
	firebird-scroll-swipe-test \
	firebird-keymap-test \
	firebird-progress-test \
	firebird-page-test \
	firebird-formats-test firebird-toc-test firebird-browser-cancel-test \
	firebird-esc-liveness-test \
	firebird-theme-test firebird-state-test firebird-font-menu-test \
	firebird-compare clean

all: desktop

MARKDOWN_FORMULA_DOCUMENT ?= /Users/ryougi/Downloads/markdown-formula.md
MEMORY_PROFILE_DOCUMENT ?= /Users/ryougi/Downloads/红楼梦.txt.tns
MEMORY_PROFILE_FONT ?= /Users/ryougi/Downloads/fusion-pixel-12px-proportional-zh_hans.ttf.tns
ESC_LIVENESS_BUILD := build/ndless-firebird-esc-liveness
ESC_LIVENESS_HEADER := $(ESC_LIVENESS_BUILD)/markdown_formula_fixture.h
SYMBOL_GALLERY_DOCUMENT := samples/math-symbol-gallery.md
SYMBOL_GALLERY_BUILD := build/ndless-firebird-symbols
SYMBOL_GALLERY_HEADER := $(SYMBOL_GALLERY_BUILD)/math_symbol_gallery_fixture.h

desktop:
	cmake -S . -B build/desktop -G Ninja
	cmake --build build/desktop

test: desktop
	ctest --test-dir build/desktop --output-on-failure

assets/core.fpk src/generated/core_font_pack.cpp: \
		assets/core-font-pack.json \
		assets/fonts/DejaVuSans-UI.ttf \
		assets/fonts/DejaVuSansMono-UI.ttf \
		assets/fonts/LatinModernMath-Core.otf \
		tools/fontpack/fontpack.py
	python3 tools/fontpack/fontpack.py assets/core-font-pack.json \
		--output assets/core.fpk --cpp-output src/generated/core_font_pack.cpp

assets/core.mfm src/generated/core_math_font.cpp: \
		assets/fonts/LatinModernMath.otf \
		assets/math-font-defaults.json \
		tools/fontpack/math_metadata.py
	python3 tools/fontpack/math_metadata.py assets/fonts/LatinModernMath.otf \
		--defaults assets/math-font-defaults.json --output assets/core.mfm \
		--cpp-output src/generated/core_math_font.cpp

generated-font-assets: assets/core.fpk src/generated/core_font_pack.cpp \
	assets/core.mfm src/generated/core_math_font.cpp

fontpack: assets/core.fpk src/generated/core_font_pack.cpp

assets/fonts/DejaVuSans-UI.ttf: assets/fonts/DejaVuSans.ttf \
		assets/fonts/DejaVuSans-UI.unicodes
	hb-subset assets/fonts/DejaVuSans.ttf \
		--output-file=assets/fonts/DejaVuSans-UI.ttf \
		--unicodes-file=assets/fonts/DejaVuSans-UI.unicodes \
		--name-IDs=1,2,4,6 --name-languages=1033

assets/fonts/DejaVuSansMono-UI.ttf: assets/fonts/DejaVuSansMono.ttf \
		assets/fonts/DejaVuSans-UI.unicodes
	hb-subset assets/fonts/DejaVuSansMono.ttf \
		--output-file=assets/fonts/DejaVuSansMono-UI.ttf \
		--unicodes-file=assets/fonts/DejaVuSans-UI.unicodes \
		--name-IDs=1,2,4,6 --name-languages=1033

ui-font-subset: assets/fonts/DejaVuSans-UI.ttf \
	assets/fonts/DejaVuSansMono-UI.ttf

assets/fonts/LatinModernMath-Core.otf: assets/fonts/LatinModernMath.otf
	hb-subset assets/fonts/LatinModernMath.otf \
		--output-file=assets/fonts/LatinModernMath-Core.otf \
		--keep-everything --name-IDs='*' \
		--name-legacy --name-languages='*'

core-math-font: assets/fonts/LatinModernMath-Core.otf

assets/fonts/DejaVuSans-CX.ttf: assets/fonts/DejaVuSans.ttf \
		assets/fonts/DejaVu-CX.unicodes
	hb-subset assets/fonts/DejaVuSans.ttf \
		--output-file=assets/fonts/DejaVuSans-CX.ttf \
		--unicodes-file=assets/fonts/DejaVu-CX.unicodes \
		--name-IDs='*' --name-legacy --name-languages='*'

assets/fonts/DejaVuSans-Oblique-CX.ttf: \
		assets/fonts/DejaVuSans-Oblique.ttf \
		assets/fonts/DejaVu-CX.unicodes
	hb-subset assets/fonts/DejaVuSans-Oblique.ttf \
		--output-file=assets/fonts/DejaVuSans-Oblique-CX.ttf \
		--unicodes-file=assets/fonts/DejaVu-CX.unicodes \
		--name-IDs='*' --name-legacy --name-languages='*'

assets/fonts/DejaVuSansMono-CX.ttf: assets/fonts/DejaVuSansMono.ttf \
		assets/fonts/DejaVu-CX.unicodes
	hb-subset assets/fonts/DejaVuSansMono.ttf \
		--output-file=assets/fonts/DejaVuSansMono-CX.ttf \
		--unicodes-file=assets/fonts/DejaVu-CX.unicodes \
		--name-IDs='*' --name-legacy --name-languages='*'

dejavu-font-subsets: assets/fonts/DejaVuSans-CX.ttf \
		assets/fonts/DejaVuSans-Oblique-CX.ttf \
		assets/fonts/DejaVuSansMono-CX.ttf

oblique-font-subset: assets/fonts/DejaVuSans-Oblique-CX.ttf

mathfont: assets/core.mfm src/generated/core_math_font.cpp

cjk-font:
	cmake -E make_directory build/fonts
	cmake -E copy_if_different assets/fonts/SarasaFixedSC-Regular-CX.ttf \
		build/fonts/SarasaFixedSC-Regular.ttf.tns

cjk-font-subset:
	hb-subset assets/fonts/SarasaFixedSC-Regular.ttf \
		--output-file=assets/fonts/SarasaFixedSC-Regular-CX.ttf \
		--unicodes-file=assets/fonts/SarasaFixedSC-CX.unicodes \
		--name-IDs='*' --name-legacy --name-languages='*'

entities:
	python3 tools/entities/generate_entities.py \
		--output src/generated/html_entities.cpp

unicode-tables:
	python3 tools/unicode/generate_unicode_tables.py --unicode-version 17.0.0 \
		--output src/generated/unicode_tables.cpp

ndless: generated-font-assets
	$(MAKE) -f Makefile.ndless

# Diagnostic-only build. It wraps the Ndless/newlib allocator and reports
# current, lifetime-peak, and most-recent-font-application heap usage in the
# Ctrl+D Reader diagnostics overlay and on stdout. The normal build has no
# tracking table or allocator interception overhead.
ndless-memory-profile: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-memory-profile \
		TARGET_NAME=nmarkdown-memory-profile \
		ZEHN_NAME=nMarkdown-Memory-Profile \
		EXTRA_CXX_SOURCES=tools/memory/ndless_allocation_probe.cpp \
		EXTRA_CPPFLAGS=-DNMARKDOWN_ALLOCATION_PROBE=1 \
		EXTRA_LDFLAGS='-Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc'

# Firebird-only allocator profile. The harness transfers the measured document
# and font to the fixed paths selected by NMARKDOWN_FIREBIRD_MEMORY_PROFILE_FIXTURE.
ndless-firebird-memory-profile: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-memory-profile \
		TARGET_NAME=nmarkdown-firebird-memory-profile \
		ZEHN_NAME=nMarkdown-Firebird-Memory-Profile \
		EXTRA_CXX_SOURCES=tools/memory/ndless_allocation_probe.cpp \
		EXTRA_CPPFLAGS='-DNMARKDOWN_ALLOCATION_PROBE=1 -DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_MEMORY_PROFILE_FIXTURE=1' \
		EXTRA_LDFLAGS='-Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc'

# Firebird GUI profile for the exact user-supplied novel and Fusion CJK font.
# The files must be transferred to /documents/ndless before launching this
# diagnostic package. No generated or equivalent-size substitute is accepted.
ndless-firebird-large-txt-profile: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-large-txt-profile \
		TARGET_NAME=nmarkdown-firebird-large-txt-profile \
		ZEHN_NAME=nMarkdown-Firebird-Large-TXT-Profile \
		EXTRA_CXX_SOURCES=tools/memory/ndless_allocation_probe.cpp \
		EXTRA_CPPFLAGS='-DNMARKDOWN_ALLOCATION_PROBE=1 -DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_REAL_NOVEL_FIXTURE=1' \
		EXTRA_LDFLAGS='-Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc'

# Exact GUI-input diagnostic. It has no wrapped touchpad sampler and no
# generated document/font fixture, so every progress marker comes from the
# production Ndless input path reading the transferred files.
ndless-firebird-real-novel: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-real-novel \
		TARGET_NAME=nmarkdown-firebird-real-novel \
		ZEHN_NAME=nMarkdown-Firebird-Real-Novel \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_REAL_NOVEL_FIXTURE=1'

ndless-firebird: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird \
		TARGET_NAME=nmarkdown-firebird-integration \
		ZEHN_NAME=nMarkdown-Firebird-Test \
		EXTRA_CPPFLAGS=-DNMARKDOWN_FIREBIRD_INTEGRATION=1

ndless-firebird-math: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-math \
		TARGET_NAME=nmarkdown-firebird-math \
		ZEHN_NAME=nMarkdown-Firebird-Math \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_MATH_FIXTURE=1'

ndless-firebird-math-review: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-math-review \
		TARGET_NAME=nmarkdown-firebird-math-review \
		ZEHN_NAME=nMarkdown-Firebird-Math-Review \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_MATH_REVIEW_FIXTURE=1'

ndless-firebird-imath: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-imath \
		TARGET_NAME=nmarkdown-firebird-imath \
		ZEHN_NAME=nMarkdown-Firebird-Bold-Italic-IJ \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_IMATH_FIXTURE=1'

$(SYMBOL_GALLERY_HEADER): $(SYMBOL_GALLERY_DOCUMENT) \
		tools/firebird/embed-document.mjs
	@mkdir -p $(dir $@)
	node tools/firebird/embed-document.mjs \
		"$(SYMBOL_GALLERY_DOCUMENT)" "$@" MathSymbolGallery

ndless-firebird-symbols: generated-font-assets $(SYMBOL_GALLERY_HEADER)
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=$(SYMBOL_GALLERY_BUILD) \
		TARGET_NAME=nmarkdown-firebird-symbols \
		ZEHN_NAME=nMarkdown-Firebird-Math-Symbols \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_SYMBOL_GALLERY_FIXTURE=1 -DNMARKDOWN_FIREBIRD_PROGRESS_FIXTURE=1 -I$(SYMBOL_GALLERY_BUILD)'

ndless-firebird-oversized-formula: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-oversized-formula \
		TARGET_NAME=nmarkdown-firebird-oversized-formula \
		ZEHN_NAME=nMarkdown-Firebird-Oversized-Formula \
		EXTRA_CXX_SOURCES=tools/firebird/oversized_formula_input_probe.cpp \
		EXTRA_LDFLAGS='-Wl,--wrap=isKeyPressed -Wl,--wrap=touchpad_scan' \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_OVERSIZED_FORMULA_FIXTURE=1'

ndless-firebird-scroll-swipe: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-scroll-swipe \
		TARGET_NAME=nmarkdown-firebird-scroll-swipe \
		ZEHN_NAME=nMarkdown-Firebird-Scroll-Swipe \
		EXTRA_CXX_SOURCES=tools/firebird/scroll_swipe_input_probe.cpp \
		EXTRA_LDFLAGS='-Wl,--wrap=isKeyPressed -Wl,--wrap=touchpad_scan' \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_SCROLL_SWIPE_FIXTURE=1 -DNMARKDOWN_FIREBIRD_PROGRESS_FIXTURE=1'

ndless-firebird-keymap: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-keymap \
		TARGET_NAME=nmarkdown-firebird-keymap \
		ZEHN_NAME=nMarkdown-Firebird-Keymap \
		EXTRA_CXX_SOURCES=tools/firebird/keymap_input_probe.cpp \
		EXTRA_LDFLAGS='-Wl,--wrap=isKeyPressed -Wl,--wrap=touchpad_scan' \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_SCROLL_SWIPE_FIXTURE=1 -DNMARKDOWN_FIREBIRD_KEYMAP_FIXTURE=1'

ndless-firebird-page: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-page \
		TARGET_NAME=nmarkdown-firebird-page \
		ZEHN_NAME=nMarkdown-Firebird-Page \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_PAGE_FIXTURE=1 -DNMARKDOWN_FIREBIRD_PROGRESS_FIXTURE=1'

ndless-firebird-formats: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-formats \
		TARGET_NAME=nmarkdown-firebird-formats \
		ZEHN_NAME=nMarkdown-Firebird-Formats \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_FORMAT_FIXTURE=1'

ndless-firebird-toc: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-toc \
		TARGET_NAME=nmarkdown-firebird-toc \
		ZEHN_NAME=nMarkdown-Firebird-TOC \
		EXTRA_CXX_SOURCES=tools/firebird/toc_input_probe.cpp \
		EXTRA_LDFLAGS='-Wl,--wrap=isKeyPressed -Wl,--wrap=touchpad_scan' \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_TOC_FIXTURE=1'

$(ESC_LIVENESS_HEADER): $(MARKDOWN_FORMULA_DOCUMENT) \
		tools/firebird/embed-document.mjs
	@mkdir -p $(dir $@)
	node tools/firebird/embed-document.mjs "$(MARKDOWN_FORMULA_DOCUMENT)" "$@"

ndless-firebird-esc-liveness: generated-font-assets $(ESC_LIVENESS_HEADER)
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=$(ESC_LIVENESS_BUILD) \
		TARGET_NAME=nmarkdown-firebird-esc-liveness \
		ZEHN_NAME=nMarkdown-Firebird-Esc-Liveness \
		EXTRA_CXX_SOURCES='tools/firebird/esc_liveness_main.cpp tools/firebird/esc_liveness_lowlevel_probe.cpp' \
		EXTRA_LDFLAGS='-Wl,--wrap=main -Wl,--wrap=isKeyPressed -Wl,--wrap=touchpad_scan' \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_TOC_FIXTURE=1 -I$(ESC_LIVENESS_BUILD)'

ndless-firebird-browser-cancel: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-browser-cancel \
		TARGET_NAME=nmarkdown-firebird-browser-cancel \
		ZEHN_NAME=nMarkdown-Firebird-Browser-Cancel \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_BROWSER_CANCEL_FIXTURE=1'

ndless-firebird-theme: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-theme \
		TARGET_NAME=nmarkdown-firebird-theme \
		ZEHN_NAME=nMarkdown-Firebird-Theme \
		EXTRA_CXX_SOURCES=tools/firebird/theme_touch_input_probe.cpp \
		EXTRA_LDFLAGS='-Wl,--wrap=touchpad_scan' \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_THEME_FIXTURE=1'

ndless-firebird-state: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-state \
		TARGET_NAME=nmarkdown-firebird-state \
		ZEHN_NAME=nMarkdown-Firebird-State \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_STATE_FIXTURE=1'

ndless-firebird-font-menu: generated-font-assets
	$(MAKE) -f Makefile.ndless \
		BUILD_DIR=build/ndless-firebird-font-menu \
		TARGET_NAME=nmarkdown-firebird-font-menu \
		ZEHN_NAME=nMarkdown-Firebird-Font-Menu \
		EXTRA_CPPFLAGS='-DNMARKDOWN_FIREBIRD_INTEGRATION=1 -DNMARKDOWN_FIREBIRD_FONT_MENU_FIXTURE=1'

firebird-test: ndless-firebird
	node tools/firebird/verify.mjs

firebird-math-test: ndless-firebird-math
	FIREBIRD_MATH_FIXTURE=1 node tools/firebird/verify.mjs

firebird-math-review-test: ndless-firebird-math-review
	FIREBIRD_MATH_REVIEW_FIXTURE=1 node tools/firebird/verify.mjs

firebird-imath-test: ndless-firebird-imath
	node tools/firebird/verify-imath.mjs

firebird-symbols-test: ndless-firebird-symbols
	node tools/firebird/verify-symbols.mjs

firebird-oversized-formula-test: ndless-firebird-oversized-formula
	FIREBIRD_OVERSIZED_FORMULA_FIXTURE=1 node tools/firebird/verify.mjs

firebird-scroll-swipe-test: ndless-firebird-scroll-swipe
	FIREBIRD_SCROLL_SWIPE_FIXTURE=1 node tools/firebird/verify.mjs

firebird-keymap-test: ndless-firebird-keymap
	node tools/firebird/verify-keymap.mjs

firebird-progress-test: ndless-firebird-scroll-swipe ndless-firebird-page
	node tools/firebird/verify-progress.mjs

firebird-page-test: ndless-firebird-page
	FIREBIRD_PAGE_FIXTURE=1 node tools/firebird/verify.mjs

firebird-formats-test: ndless-firebird-formats
	FIREBIRD_FORMAT_FIXTURE=1 node tools/firebird/verify.mjs

firebird-toc-test: ndless-firebird-toc
	FIREBIRD_TOC_FIXTURE=1 node tools/firebird/verify.mjs

firebird-esc-liveness-test: ndless-firebird-esc-liveness
	MARKDOWN_FORMULA_DOCUMENT="$(MARKDOWN_FORMULA_DOCUMENT)" \
		node tools/firebird/verify-esc-liveness.mjs

firebird-browser-cancel-test: ndless-firebird-browser-cancel
	FIREBIRD_BROWSER_CANCEL_FIXTURE=1 node tools/firebird/verify.mjs

firebird-theme-test: ndless-firebird-theme
	FIREBIRD_THEME_FIXTURE=1 node tools/firebird/verify.mjs

firebird-state-test: ndless-firebird-state
	FIREBIRD_STATE_FIXTURE=1 node tools/firebird/verify.mjs

firebird-font-menu-test: ndless-firebird-font-menu
	FIREBIRD_FONT_MENU_FIXTURE=1 node tools/firebird/verify.mjs

firebird-compare:
	node tools/firebird/compare-server.mjs

clean:
	cmake -E rm -rf build
