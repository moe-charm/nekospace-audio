# NekoSpace Reverb — Phase 0 analysis harness.
#
# This is deliberately JUCE-free and deliberately depends on nsb_dsp: Phase 0 measures the
# exact existing 8-line implementation where it lives. The independent product core is a separate
# target; this analyzer remains the immutable comparison path.

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

# Room Body audition product: one processor/editor builds both VST3 and JUCE Standalone.
juce_add_plugin(NekoSpaceReverb
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "NekoSpace Audio"
    BUNDLE_ID "audio.nekospace.reverb"
    PLUGIN_MANUFACTURER_CODE NkSp
    PLUGIN_CODE Nksr
    FORMATS VST3 Standalone
    PRODUCT_NAME "NekoSpace Reverb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Reverb")
target_sources(NekoSpaceReverb PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp)
target_compile_definitions(NekoSpaceReverb PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(NekoSpaceReverb PRIVATE nsr_dsp juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

# A file/device transport around the exact processor/editor above. No product controls
# or DSP are duplicated in this target.
juce_add_gui_app(NekoSpaceReverbPlayer
    PRODUCT_NAME "NekoSpace Reverb Player"
    COMPANY_NAME "NekoSpace Audio"
    BUNDLE_ID "audio.nekospace.reverb-player"
    VERSION ${PROJECT_VERSION})
target_sources(NekoSpaceReverbPlayer PRIVATE player/Main.cpp)
target_compile_definitions(NekoSpaceReverbPlayer PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(NekoSpaceReverbPlayer PRIVATE
    NekoSpaceReverb nsr_dsp juce::juce_audio_utils juce::juce_gui_extra
    juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

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

add_executable(nsr_plugin_tests tests/PluginTests.cpp)
target_link_libraries(nsr_plugin_tests PRIVATE NekoSpaceReverb nsr_dsp juce::juce_audio_utils)
add_test(NAME reverb_plugin COMMAND nsr_plugin_tests)

add_executable(nsr_phase3_compare tools/phase3_compare.cpp)
target_link_libraries(nsr_phase3_compare PRIVATE nsr_analysis)

# Manual Release benchmark: the real processor callback, 30 minutes of simulated 48 kHz / 64
# sample automation, allocation/free instrumentation, latency percentiles and process memory.
# It is deliberately not a CTest because shared CI hardware cannot be a stable realtime gate.
add_executable(nsr_realtime_bench tools/realtime_bench.cpp)
target_link_libraries(nsr_realtime_bench PRIVATE
    NekoSpaceReverb nsr_analysis juce::juce_audio_utils)
if(WIN32)
    target_link_libraries(nsr_realtime_bench PRIVATE Psapi Avrt)
endif()

if(MSVC)
    target_compile_options(NekoSpaceReverb PRIVATE /utf-8)
    target_compile_options(nekospace_reverb_analyze PRIVATE /W4 /utf-8)
    target_compile_options(nsr_analysis_tests PRIVATE /W4 /utf-8)
    target_compile_options(nsr_dsp_tests PRIVATE /W4 /utf-8)
    target_compile_options(nsr_plugin_tests PRIVATE /W4 /utf-8)
    target_compile_options(nsr_phase3_compare PRIVATE /W4 /utf-8)
    target_compile_options(nsr_realtime_bench PRIVATE /W4 /utf-8)
endif()
