# NekoSpace Binaural. JUCE is fetched once by the top-level CMakeLists.

# --- JUCE-free DSP core (Contract #4) ---
add_library(nsb_dsp INTERFACE)
target_include_directories(nsb_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- Plugin ---
juce_add_plugin(NekoSpaceBinaural
    COMPANY_NAME "NekoSpace Audio"
    PLUGIN_MANUFACTURER_CODE NkSp
    PLUGIN_CODE Nksb
    FORMATS VST3 Standalone
    PRODUCT_NAME "NekoSpace Binaural"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Spatial"
)

target_sources(NekoSpaceBinaural PRIVATE
    src/plugin/PluginProcessor.cpp
)

# --- Optional measured HRTF pack (development listening prototype) ---
# OFF by default: the TH Köln KU100 data is CC BY-SA and is NOT part of the distributed
# plugin until it wins the listening comparison. See docs/elevation-findings.md.
#   python tools/hrtf-pack/sofa_to_bhrtf.py <in.sofa> resources/hrtf/ku100_48k.bhrtf
#   cmake -B build -DNSB_WITH_KU100=ON
option(NSB_WITH_KU100 "Embed the experimental measured KU100 HRTF pack (dev only)" OFF)
if(NSB_WITH_KU100)
    set(NSB_KU100_PACK "${CMAKE_CURRENT_SOURCE_DIR}/resources/hrtf/ku100_48k.bhrtf")
    if(NOT EXISTS "${NSB_KU100_PACK}")
        message(FATAL_ERROR
            "NSB_WITH_KU100=ON but ${NSB_KU100_PACK} is missing.\n"
            "Generate it first:\n"
            "  python tools/hrtf-pack/sofa_to_bhrtf.py <HRIR_L2702.sofa> ${NSB_KU100_PACK}")
    endif()
    juce_add_binary_data(NekoSpaceHrtfData SOURCES "${NSB_KU100_PACK}")
    target_link_libraries(NekoSpaceBinaural PRIVATE NekoSpaceHrtfData)
    target_compile_definitions(NekoSpaceBinaural PRIVATE NSB_WITH_KU100=1)
endif()

target_compile_definitions(NekoSpaceBinaural PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(NekoSpaceBinaural
    PRIVATE
        nsb_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(NekoSpaceBinaural PRIVATE /utf-8)
endif()

# --- JUCE-free DSP tests (Contract #12) ---
add_executable(nsb_tests tests/DspTests.cpp)
target_link_libraries(nsb_tests PRIVATE nsb_dsp)
if(MSVC)
    target_compile_options(nsb_tests PRIVATE /W4 /utf-8)
endif()
# run from the plugin directory so the optional real-pack check can find resources/
add_test(NAME binaural_dsp COMMAND nsb_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
