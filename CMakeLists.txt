cmake_minimum_required(VERSION 3.20)

project(nMarkdown VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NMARKDOWN_CJK_FONT_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/SarasaFixedSC-Regular-CX.ttf")
set(NMARKDOWN_CJK_FONT_ARTIFACT
    "${CMAKE_CURRENT_BINARY_DIR}/fonts/SarasaFixedSC-Regular.ttf.tns")
add_custom_command(
    OUTPUT "${NMARKDOWN_CJK_FONT_ARTIFACT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/fonts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${NMARKDOWN_CJK_FONT_SOURCE}"
            "${NMARKDOWN_CJK_FONT_ARTIFACT}"
    DEPENDS "${NMARKDOWN_CJK_FONT_SOURCE}"
    COMMENT "Packaging transferable Sarasa Fixed SC CJK font"
    VERBATIM
)
add_custom_target(nmarkdown-cjk-font
    DEPENDS "${NMARKDOWN_CJK_FONT_ARTIFACT}")

add_library(freetype-minimal STATIC
    third_party/freetype/src/base/ftsystem.c
    third_party/freetype/src/base/ftdebug.c
    third_party/freetype/src/base/ftinit.c
    third_party/freetype/src/base/ftbase.c
    third_party/freetype/src/base/ftbitmap.c
    third_party/freetype/src/base/ftmm.c
    third_party/freetype/src/sfnt/sfnt.c
    third_party/freetype/src/truetype/truetype.c
    third_party/freetype/src/cff/cff.c
    third_party/freetype/src/psaux/psaux.c
    third_party/freetype/src/pshinter/pshinter.c
    third_party/freetype/src/psnames/psnames.c
    third_party/freetype/src/smooth/smooth.c
    third_party/freetype/src/autofit/autofit.c
)
target_include_directories(freetype-minimal PUBLIC
    third_party/freetype/nmarkdown
    third_party/freetype/include
)
target_compile_definitions(freetype-minimal PRIVATE FT2_BUILD_LIBRARY)
target_compile_options(freetype-minimal PRIVATE -Os)

add_library(md4c STATIC third_party/md4c/md4c.c)
target_include_directories(md4c PUBLIC third_party/md4c)
target_compile_options(md4c PRIVATE -Os -Wall -Wextra)

add_library(harfbuzz STATIC third_party/harfbuzz/src/harfbuzz.cc)
target_include_directories(harfbuzz PUBLIC third_party/harfbuzz/src)
target_compile_definitions(harfbuzz PRIVATE
    HB_TINY=1
    NMARKDOWN_HARFBUZZ_MATH=1
)
target_compile_options(harfbuzz PRIVATE -Os)

add_library(nmarkdown_core
    src/app/application.cpp
    src/app/viewer.cpp
    src/document/document_ir.cpp
    src/document/entity.cpp
    src/document/markdown.cpp
    src/document/search.cpp
    src/document/state.cpp
    src/document/text_encoding.cpp
    src/document/unicode.cpp
    src/document/utf8.cpp
    src/generated/core_font_pack.cpp
    src/generated/html_entities.cpp
    src/generated/core_math_font.cpp
    src/generated/unicode_tables.cpp
    src/layout/block_layout.cpp
    src/layout/fenwick.cpp
    src/layout/plain_text_layout.cpp
    src/math/math_atoms.cpp
    src/math/math_lexer.cpp
    src/math/math_layout.cpp
    src/math/math_macros.cpp
    src/math/math_parser.cpp
    src/math/math_system.cpp
    src/platform/stdio_files.cpp
    src/render/primitives.cpp
    src/render/surface565.cpp
    src/text/compositor.cpp
    src/text/font.cpp
    src/text/font_catalog.cpp
    src/text/font_pack.cpp
    src/text/glyph_cache.cpp
    src/text/harfbuzz_shaper.cpp
    src/text/text_renderer.cpp
    src/text/text_system.cpp
)

target_include_directories(nmarkdown_core PUBLIC include)
target_link_libraries(nmarkdown_core PUBLIC freetype-minimal md4c harfbuzz)
target_compile_options(nmarkdown_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(nmarkdown-desktop
    src/platform/desktop/clock_desktop.cpp
    src/platform/desktop/display_desktop.cpp
    src/platform/desktop/input_desktop.cpp
    src/platform/desktop/main.cpp
)

target_include_directories(nmarkdown-desktop PRIVATE src/platform/desktop)
target_link_libraries(nmarkdown-desktop PRIVATE nmarkdown_core)
target_compile_options(nmarkdown-desktop PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    # Manual corpus benchmark. It is intentionally not a ctest because the
    # required large book/font paths are local inputs rather than fixtures.
    add_executable(nmarkdown-large-txt-bench
        tools/perf/large_txt_bench.cpp)
    target_link_libraries(nmarkdown-large-txt-bench PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-large-txt-bench PRIVATE
        -Wall -Wextra -Wpedantic)

    add_executable(nmarkdown-tests tests/test_phase0.cpp)
    add_dependencies(nmarkdown-tests nmarkdown-cjk-font)
    target_link_libraries(nmarkdown-tests PRIVATE nmarkdown_core)
    target_compile_definitions(nmarkdown-tests PRIVATE
        NMARKDOWN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        NMARKDOWN_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
    )
    target_compile_options(nmarkdown-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase0-core COMMAND nmarkdown-tests)

    add_executable(nmarkdown-viewer-interaction-tests
        tests/test_viewer_interactions.cpp)
    target_link_libraries(nmarkdown-viewer-interaction-tests
        PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-viewer-interaction-tests
        PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase0-viewer-interactions
        COMMAND nmarkdown-viewer-interaction-tests)

    # Compile the real Ndless input adapter against a deterministic host stub.
    # This pins key-release behavior without requiring a calculator or
    # weakening the Firebird/real-hardware distinction.
    add_executable(nmarkdown-input-ndless-tests
        tests/test_input_ndless.cpp
        src/platform/ndless/input_ndless.cpp)
    target_include_directories(nmarkdown-input-ndless-tests PRIVATE
        tests/fake_ndless
        src/platform/ndless
        include)
    target_compile_options(nmarkdown-input-ndless-tests PRIVATE
        -Wall -Wextra -Wpedantic)
    add_test(NAME phase0-input-ndless-liveness
        COMMAND nmarkdown-input-ndless-tests)

    # Compile the real calculator clock against the host Ndless shim. The shim
    # intentionally has no msleep declaration, so reintroducing the unbounded
    # SDK WFI path is also a build failure.
    add_executable(nmarkdown-clock-ndless-tests
        tests/test_clock_ndless.cpp
        src/platform/ndless/clock_ndless.cpp)
    target_include_directories(nmarkdown-clock-ndless-tests PRIVATE
        tests/fake_ndless
        src/platform/ndless
        include)
    target_compile_options(nmarkdown-clock-ndless-tests PRIVATE
        -Wall -Wextra -Wpedantic)
    add_test(NAME phase0-clock-ndless-bounded
        COMMAND nmarkdown-clock-ndless-tests)

    # Pin the CX II native staging orientation and ensure the Vsync gate never
    # accepts a vertical-compare bit that was already latched at entry.
    add_executable(nmarkdown-display-sync-tests
        tests/test_display_sync.cpp)
    target_include_directories(nmarkdown-display-sync-tests PRIVATE include)
    target_compile_options(nmarkdown-display-sync-tests PRIVATE
        -Wall -Wextra -Wpedantic)
    add_test(NAME phase0-display-sync
        COMMAND nmarkdown-display-sync-tests)

    add_executable(nmarkdown-menu-layout-tests tests/test_menu_layout.cpp)
    target_link_libraries(nmarkdown-menu-layout-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-menu-layout-tests PRIVATE
        -Wall -Wextra -Wpedantic)
    add_test(NAME phase0-menu-layout COMMAND nmarkdown-menu-layout-tests)

    # Compile stdio_files.cpp separately so this target can emulate the
    # ENOSYS rename behavior exposed by Ndless/newlib on supported TI OSes.
    add_executable(nmarkdown-stdio-rename-fallback-tests
        tests/test_stdio_rename_fallback.cpp
        src/platform/stdio_files.cpp)
    target_include_directories(nmarkdown-stdio-rename-fallback-tests
        PRIVATE include)
    target_compile_definitions(nmarkdown-stdio-rename-fallback-tests PRIVATE
        NMARKDOWN_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        NMARKDOWN_TEST_RENAME_ENOSYS=1)
    target_compile_options(nmarkdown-stdio-rename-fallback-tests
        PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase0-stdio-rename-fallback
        COMMAND nmarkdown-stdio-rename-fallback-tests)

    add_executable(nmarkdown-utf8-tests tests/test_utf8.cpp)
    target_link_libraries(nmarkdown-utf8-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-utf8-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase1-utf8 COMMAND nmarkdown-utf8-tests)

    add_executable(nmarkdown-text-encoding-tests
        tests/test_text_encoding.cpp)
    target_link_libraries(nmarkdown-text-encoding-tests PRIVATE nmarkdown_core)
    target_compile_definitions(nmarkdown-text-encoding-tests PRIVATE
        NMARKDOWN_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_options(nmarkdown-text-encoding-tests PRIVATE
        -Wall -Wextra -Wpedantic)
    add_test(NAME phase1-text-encoding COMMAND nmarkdown-text-encoding-tests)

    add_executable(nmarkdown-unicode-tests tests/test_unicode.cpp)
    target_link_libraries(nmarkdown-unicode-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-unicode-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase5-unicode COMMAND nmarkdown-unicode-tests)

    add_executable(nmarkdown-font-pack-tests tests/test_font_pack.cpp)
    target_link_libraries(nmarkdown-font-pack-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-font-pack-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase1-font-pack COMMAND nmarkdown-font-pack-tests)

    add_executable(nmarkdown-font-catalog-tests tests/test_font_catalog.cpp)
    target_link_libraries(nmarkdown-font-catalog-tests PRIVATE nmarkdown_core)
    target_compile_definitions(nmarkdown-font-catalog-tests PRIVATE
        NMARKDOWN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(nmarkdown-font-catalog-tests PRIVATE
        -Wall -Wextra -Wpedantic)
    add_test(NAME phase1-font-catalog COMMAND nmarkdown-font-catalog-tests)

    add_executable(nmarkdown-text-tests tests/test_text.cpp)
    target_link_libraries(nmarkdown-text-tests PRIVATE nmarkdown_core)
    target_compile_definitions(nmarkdown-text-tests PRIVATE
        NMARKDOWN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_options(nmarkdown-text-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase1-text COMMAND nmarkdown-text-tests)

    add_executable(nmarkdown-markdown-tests tests/test_markdown.cpp)
    target_link_libraries(nmarkdown-markdown-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-markdown-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase2-markdown COMMAND nmarkdown-markdown-tests)

    add_executable(nmarkdown-fenwick-tests tests/test_fenwick.cpp)
    target_link_libraries(nmarkdown-fenwick-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-fenwick-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase2-fenwick COMMAND nmarkdown-fenwick-tests)

    add_executable(nmarkdown-block-layout-tests tests/test_block_layout.cpp)
    target_link_libraries(nmarkdown-block-layout-tests PRIVATE nmarkdown_core)
    target_compile_definitions(nmarkdown-block-layout-tests PRIVATE
        NMARKDOWN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(nmarkdown-block-layout-tests PRIVATE
        NMARKDOWN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_options(nmarkdown-block-layout-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase2-block-layout COMMAND nmarkdown-block-layout-tests)

    add_executable(nmarkdown-plain-text-layout-tests
        tests/test_plain_text_layout.cpp)
    target_link_libraries(nmarkdown-plain-text-layout-tests
        PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-plain-text-layout-tests PRIVATE
        -Wall -Wextra -Wpedantic)
    add_test(NAME phase2-plain-text-layout
        COMMAND nmarkdown-plain-text-layout-tests)

    add_executable(nmarkdown-state-tests tests/test_state.cpp)
    target_link_libraries(nmarkdown-state-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-state-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase2-state COMMAND nmarkdown-state-tests)

    add_executable(nmarkdown-search-tests tests/test_search.cpp)
    target_link_libraries(nmarkdown-search-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-search-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase4-search COMMAND nmarkdown-search-tests)

    add_executable(nmarkdown-math-parser-tests tests/test_math_parser.cpp)
    target_link_libraries(nmarkdown-math-parser-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-math-parser-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase3-math-parser COMMAND nmarkdown-math-parser-tests)

    add_executable(nmarkdown-math-layout-tests tests/test_math_layout.cpp)
    target_link_libraries(nmarkdown-math-layout-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-math-layout-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase3-math-layout COMMAND nmarkdown-math-layout-tests)

    add_executable(nmarkdown-math-golden-tests tests/test_math_golden.cpp)
    target_link_libraries(nmarkdown-math-golden-tests PRIVATE nmarkdown_core)
    target_compile_options(nmarkdown-math-golden-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME phase3-math-golden COMMAND nmarkdown-math-golden-tests)

    add_test(
        NAME phase0-desktop-harness
        COMMAND nmarkdown-desktop
            --document ${CMAKE_CURRENT_SOURCE_DIR}/samples/phase0.md
            --no-state
            --events menu,page-down
            --output ${CMAKE_CURRENT_BINARY_DIR}/phase0-ctest.ppm
    )
    set_tests_properties(phase0-desktop-harness PROPERTIES TIMEOUT 5)

    add_test(
        NAME phase0-direct-md-browser
        COMMAND nmarkdown-desktop
            --browse
            --root ${CMAKE_CURRENT_SOURCE_DIR}/samples
            --no-state
            --events enter
            --output ${CMAKE_CURRENT_BINARY_DIR}/direct-md-browser-ctest.ppm
    )
    set_tests_properties(phase0-direct-md-browser PROPERTIES TIMEOUT 5)

    add_test(
        NAME phase4-raw-markdown-harness
        COMMAND nmarkdown-desktop
            --document ${CMAKE_CURRENT_SOURCE_DIR}/samples/phase4.md
            --no-state
            --events search,text:unicode,right,right,enter,settings,down,down,enter,back,diagnostics
            --output ${CMAKE_CURRENT_BINARY_DIR}/phase4-raw-ctest.ppm
    )
    set_tests_properties(phase4-raw-markdown-harness PROPERTIES TIMEOUT 5)
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(fontpack
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/fontpack/fontpack.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/core-font-pack.json
            --output ${CMAKE_CURRENT_SOURCE_DIR}/assets/core.fpk
            --cpp-output ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/core_font_pack.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Regenerating the embedded core font pack"
    )
    add_custom_target(entities
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/entities/generate_entities.py
            --output ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/html_entities.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Regenerating the HTML5 named-entity table"
    )
    add_custom_target(mathfont
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/fontpack/math_metadata.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/LatinModernMath.otf
            --defaults ${CMAKE_CURRENT_SOURCE_DIR}/assets/math-font-defaults.json
            --output ${CMAKE_CURRENT_SOURCE_DIR}/assets/core.mfm
            --cpp-output ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/core_math_font.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Regenerating Latin Modern mathematical font constants"
    )
    add_custom_target(unicode-tables
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/unicode/generate_unicode_tables.py
            --unicode-version 17.0.0
            --output ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/unicode_tables.cpp
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Regenerating Unicode 17 case-folding and normalization tables"
    )
endif()
