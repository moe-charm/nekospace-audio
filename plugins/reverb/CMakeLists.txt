# NekoSpace Reverb — Phase 0 analysis harness.
#
# This is deliberately JUCE-free and deliberately depends on nsb_dsp: Phase 0 measures the
# exact existing 8-line implementation where it lives. Shared extraction starts only in
# Phase 1, after the baseline has been frozen.

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE NSR_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT NSR_GIT_COMMIT)
    set(NSR_GIT_COMMIT "unknown")
endif()
execute_process(
    COMMAND git status --porcelain
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE NSR_GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NSR_GIT_STATUS)
    set(NSR_GIT_DIRTY 1)
else()
    set(NSR_GIT_DIRTY 0)
endif()

add_library(nsr_dsp INTERFACE)
target_include_directories(nsr_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(nsr_dsp INTERFACE nekospace_dsp)
target_compile_features(nsr_dsp INTERFACE cxx_std_17)

add_library(nsr_analysis INTERFACE)
target_include_directories(nsr_analysis INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(nsr_analysis INTERFACE nsb_dsp nsr_dsp)
target_compile_features(nsr_analysis INTERFACE cxx_std_17)
target_compile_definitions(nsr_analysis INTERFACE
    NSR_GIT_COMMIT="${NSR_GIT_COMMIT}"
    NSR_GIT_DIRTY=${NSR_GIT_DIRTY}
    NSR_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
    NSR_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
    NSR_BUILD_CONFIG="$<CONFIG>")

add_executable(nekospace_reverb_analyze tools/analyze.cpp)
target_link_libraries(nekospace_reverb_analyze PRIVATE nsr_analysis)

add_executable(nsr_analysis_tests tests/AnalysisTests.cpp)
target_link_libraries(nsr_analysis_tests PRIVATE nsr_analysis)
add_test(NAME reverb_analysis COMMAND nsr_analysis_tests)

# A fast end-to-end check that exercises directory creation, WAV and all text reports.
add_test(NAME reverb_analysis_smoke
    COMMAND nekospace_reverb_analyze
        --output "${CMAKE_CURRENT_BINARY_DIR}/smoke-report"
        --duration 1.0
        --decay 0.35
        --block-size 127)

add_executable(nsr_dsp_tests tests/DspTests.cpp)
target_link_libraries(nsr_dsp_tests PRIVATE nsr_dsp)
add_test(NAME reverb_dsp COMMAND nsr_dsp_tests)

if(MSVC)
    target_compile_options(nekospace_reverb_analyze PRIVATE /W4 /utf-8)
    target_compile_options(nsr_analysis_tests PRIVATE /W4 /utf-8)
    target_compile_options(nsr_dsp_tests PRIVATE /W4 /utf-8)
endif()
