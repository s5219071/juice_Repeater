cmake_minimum_required(VERSION 3.22)

project(JuiceRepeater VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

include(FetchContent)

set(JUICEREPEATER_JUCE_VERSION "8.0.12" CACHE STRING "JUCE version fetched by CMake")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Disable JUCE examples" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Disable JUCE extras" FORCE)

FetchContent_Declare(
    JUCE
    URL "https://github.com/juce-framework/JUCE/archive/refs/tags/${JUICEREPEATER_JUCE_VERSION}.zip"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(JuiceRepeater
    VERSION 1.0.0
    COMPANY_NAME "Kino"
    COMPANY_WEBSITE "https://github.com"
    COMPANY_EMAIL "kino@example.com"

    FORMATS VST3
    VST3_CATEGORIES Fx Delay

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE

    PLUGIN_MANUFACTURER_CODE Kino
    PLUGIN_CODE JRep

    PRODUCT_NAME "JuiceRepeater"
    BUNDLE_ID "com.kino.juicerepeater"
)

target_sources(JuiceRepeater
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

target_link_libraries(JuiceRepeater
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

juce_generate_juce_header(JuiceRepeater)

target_compile_definitions(JuiceRepeater
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
