cmake_minimum_required(VERSION 3.22)

project(Playground VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static runtime on Windows: the plugin then works without installing the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(PLAYGROUND_BUILD_TESTS "Build the headless test program" OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

if(WIN32 OR APPLE)
    set(PLAYGROUND_FORMATS VST3 Standalone)
else()
    set(PLAYGROUND_FORMATS VST3)
endif()

juce_add_plugin(Playground
    COMPANY_NAME                "Playground Audio"
    BUNDLE_ID                   com.playgroundaudio.playground
    PRODUCT_NAME                "Playground"
    PLUGIN_MANUFACTURER_CODE    Plgd
    PLUGIN_CODE                 Pgrd
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    FORMATS                     ${PLAYGROUND_FORMATS}
    VST3_CATEGORIES             Instrument Synth)

set(PLAYGROUND_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Synth.h
    Source/Scales.h)

target_sources(Playground PRIVATE ${PLAYGROUND_SOURCES})

target_compile_definitions(Playground PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Playground
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(PLAYGROUND_BUILD_TESTS)
    juce_add_console_app(PlaygroundTests PRODUCT_NAME "PlaygroundTests")
    target_sources(PlaygroundTests PRIVATE Tests/PlaygroundTests.cpp ${PLAYGROUND_SOURCES})
    target_compile_definitions(PlaygroundTests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(PlaygroundTests
        PRIVATE juce::juce_audio_utils
        PUBLIC  juce::juce_recommended_config_flags)
endif()
