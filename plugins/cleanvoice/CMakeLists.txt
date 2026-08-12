# NekoSpace CleanVoice — v1 prototype.
#
# Deliberately does NOT link JUCE. This is a command-line tool over a file; pulling a
# framework in for WAV I/O would slow every build and buy nothing. The DSP core is
# JUCE-free by Architecture Contract #4 anyway, so when a plugin version is wanted the
# same headers are what it will use.

add_library(cv_dsp INTERFACE)
target_include_directories(cv_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(cleanvoice tools/main.cpp)
target_link_libraries(cleanvoice PRIVATE cv_dsp)
target_compile_features(cleanvoice PRIVATE cxx_std_17)

add_executable(cv_tests tests/DspTests.cpp)
target_link_libraries(cv_tests PRIVATE cv_dsp)
target_compile_features(cv_tests PRIVATE cxx_std_17)
add_test(NAME cleanvoice_dsp COMMAND cv_tests)

add_executable(cv_session_tests tests/SessionTests.cpp)
target_link_libraries(cv_session_tests PRIVATE cv_dsp)
target_compile_features(cv_session_tests PRIVATE cxx_std_17)
add_test(NAME cleanvoice_session COMMAND cv_session_tests)

if(MSVC)
    target_compile_options(cleanvoice PRIVATE /utf-8)
    target_compile_options(cv_tests PRIVATE /W4 /utf-8)
endif()

# --- GUI app ---
# JUCE is linked HERE and only here. cv_dsp above stays framework-free, which is what
# makes a VST3 version later a matter of adding a wrapper rather than untangling one.
juce_add_gui_app(CleanVoiceApp
    PRODUCT_NAME "NekoSpace CleanVoice"
    COMPANY_NAME "NekoSpace Audio"
    BUNDLE_ID "audio.nekospace.cleanvoice"
    VERSION ${PROJECT_VERSION})

target_sources(CleanVoiceApp PRIVATE app/Main.cpp)
target_compile_definitions(CleanVoiceApp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CleanVoiceApp,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CleanVoiceApp,JUCE_VERSION>")
target_link_libraries(CleanVoiceApp PRIVATE
    cv_dsp
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

if(MSVC)
    target_compile_options(CleanVoiceApp PRIVATE /utf-8)
endif()
