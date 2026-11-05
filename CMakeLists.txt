cmake_minimum_required(VERSION 3.22)

project(LiveKeys VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE via FetchContent (github.com/juce-framework/JUCE) ---
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
set(JUCE_ENABLE_WEB_BROWSER OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(LiveKeys
    PRODUCT_NAME "LiveKeys"
    COMPANY_NAME "LiveKeys"
    BUNDLE_ID    "com.livekeys.app"
    NEEDS_MIDI_INPUT TRUE
    NEEDS_CURL FALSE
)

juce_generate_juce_header(LiveKeys)

target_sources(LiveKeys PRIVATE
    src/Main.cpp
    src/core/AudioEngine.cpp
    src/core/PatchManager.cpp
    src/core/PluginRack.cpp
    src/core/MidiRouter.cpp
    src/core/MidiClockSync.cpp
    src/core/RemoteControl.cpp
    src/core/Persistence.cpp
    src/ui/MainWindow.cpp
)

target_compile_definitions(LiveKeys PRIVATE
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    # LV2 support needs the system headers (lv2-dev on Arch)
    JUCE_PLUGINHOST_LV2=1
    JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(LiveKeys PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_osc
    juce::juce_recommended_config_flags
)
