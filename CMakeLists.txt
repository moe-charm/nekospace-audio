cmake_minimum_required(VERSION 3.22)
project(NekoSpaceAudio VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE (pinned tag — Architecture Contract: never track master) ---
# Fetched once here and shared by every plugin in the repository.
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

enable_testing()

# --- Shared code ---
# Product-neutral code with at least two proven consumers; see shared/README.md.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shared/CMakeLists.txt")
    add_subdirectory(shared)
endif()

# --- Products ---
add_subdirectory(plugins/binaural)
# CleanVoice's DSP and CLI stay JUCE-free; its standalone GUI links JUCE only at the edge.
add_subdirectory(plugins/cleanvoice)
# Reverb owns an independent JUCE-free Phase 1 core and retains the Phase 0 analyzer for
# comparisons against Binaural's frozen 8-line baseline.
add_subdirectory(plugins/reverb)
