# Product-neutral, JUCE-free DSP primitives shared by two or more products.
add_library(nekospace_dsp INTERFACE)
target_include_directories(nekospace_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(nekospace_dsp INTERFACE cxx_std_17)
