cmake_minimum_required (VERSION 3.22)
project (VintageRack VERSION 1.0.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
# Universal binary (Apple Silicon + Intel) on macOS
set (CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# --- JUCE: use a local checkout if given (-DVR_JUCE_PATH=...), otherwise download JUCE 8.0.4
set (VR_JUCE_PATH "" CACHE PATH "Path to an existing JUCE checkout (optional)")
option (VR_COPY_AFTER_BUILD "Install plugins into the system plug-in folders after building" OFF)

if (VR_JUCE_PATH)
    add_subdirectory (${VR_JUCE_PATH} JUCE)
else()
    include (FetchContent)
    FetchContent_Declare (JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable (JUCE)
endif()

# factory presets (shared with the web version)
juce_add_binary_data (VRData HEADER_NAME BinaryData.h NAMESPACE VRData SOURCES Resources/presets.json)

# vr_add_plugin(<target> <product name> <4-char code> <module bitmask: 1=73, 2=76, 4=3A>)
function (vr_add_plugin target product code modules)
    juce_add_plugin (${target}
        COMPANY_NAME "Vintage Rack"
        PRODUCT_NAME "${product}"
        BUNDLE_ID "com.vintagerack.${target}"
        PLUGIN_MANUFACTURER_CODE VtRk
        PLUGIN_CODE ${code}
        FORMATS AU VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES Fx Dynamics EQ
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD ${VR_COPY_AFTER_BUILD})

    target_sources (${target} PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions (${target} PUBLIC
        VR_MODULES=${modules}
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    if (MSVC)
        target_compile_options (${target} PRIVATE /utf-8)   # Japanese UI strings are UTF-8
    endif()

    target_link_libraries (${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            VRData
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

vr_add_plugin (VintageRack "Vintage Rack" VRak 7)   # all three, reorderable
vr_add_plugin (VR73        "VR Pre EQ"       VR73 1)   # Pre / EQ only
vr_add_plugin (VR76        "VR FET Comp"     VR76 2)   # FET compressor only
vr_add_plugin (VR3A        "VR Opto Leveler" VR3a 4)   # Opto leveler only

# Offscreen test (render UI + run audio). Not needed for normal builds.
option (VR_BUILD_TESTS "Build the offscreen snapshot test" OFF)
if (VR_BUILD_TESTS)
    juce_add_console_app (VRSnapshot PRODUCT_NAME "VRSnapshot")
    target_sources (VRSnapshot PRIVATE tests/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions (VRSnapshot PRIVATE VR_MODULES=7 JucePlugin_Name="Vintage Rack" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries (VRSnapshot PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors VRData PUBLIC juce::juce_recommended_config_flags)
endif()
