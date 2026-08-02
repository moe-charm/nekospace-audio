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
