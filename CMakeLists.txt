cmake_minimum_required(VERSION 3.22)

project(Hexstack VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HEXSTACK_COPY_PLUGIN_AFTER_BUILD "Copy plugin builds into the local DELIVERABLES folder after build" ON)
option(HEXSTACK_ENABLE_VST2 "Enable legacy VST2 plugin target when a compatible SDK is available" OFF)
set(HEXSTACK_VST2_SDK_PATH "" CACHE PATH "Path to the legacy VST2 SDK")

if(NOT HEXSTACK_VST2_SDK_PATH AND DEFINED ENV{HEXSTACK_VST2_SDK_PATH} AND NOT "$ENV{HEXSTACK_VST2_SDK_PATH}" STREQUAL "")
    set(HEXSTACK_VST2_SDK_PATH "$ENV{HEXSTACK_VST2_SDK_PATH}" CACHE PATH "Path to the legacy VST2 SDK" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
    juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
    signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        main
    GIT_SHALLOW    ON
)

FetchContent_MakeAvailable(juce signalsmith-stretch)

# Patch JUCE standalone so "Mute audio input" defaults to OFF.
# A guitar amp plugin always requires live input — the user is responsible for
# their monitoring chain. Without this patch the standalone silences the input
# on every first launch (JUCE default = true to prevent speaker feedback).
set(_juce_standalone_h
    "${juce_SOURCE_DIR}/modules/juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h")
if(EXISTS "${_juce_standalone_h}")
    file(READ "${_juce_standalone_h}" _juce_standalone_content)
    string(REPLACE
        "settings->getBoolValue (\"shouldMuteInput\", true)"
        "settings->getBoolValue (\"shouldMuteInput\", false)"
        _juce_standalone_patched "${_juce_standalone_content}")
    if(NOT _juce_standalone_content STREQUAL _juce_standalone_patched)
        file(WRITE "${_juce_standalone_h}" "${_juce_standalone_patched}")
        message(STATUS "Hexstack: patched JUCE standalone — muteInput defaults to OFF")
    endif()
endif()

set(HEXSTACK_DELIVERABLES_DIR "${CMAKE_SOURCE_DIR}/DELIVERABLES")
set(HEXSTACK_PLUGIN_FORMATS Standalone VST3)
set(HEXSTACK_PLUGIN_COPY_ARGS)

if(WIN32)
    set(HEXSTACK_PLATFORM_DELIVERABLES_DIR "${HEXSTACK_DELIVERABLES_DIR}/Windows")
elseif(APPLE)
    set(HEXSTACK_PLATFORM_DELIVERABLES_DIR "${HEXSTACK_DELIVERABLES_DIR}/macOS")
else()
    set(HEXSTACK_PLATFORM_DELIVERABLES_DIR "${HEXSTACK_DELIVERABLES_DIR}/Other")
endif()

if(HEXSTACK_COPY_PLUGIN_AFTER_BUILD)
    list(APPEND HEXSTACK_PLUGIN_COPY_ARGS COPY_PLUGIN_AFTER_BUILD TRUE)

    if(WIN32)
        list(APPEND HEXSTACK_PLUGIN_COPY_ARGS
            VST3_COPY_DIR "${HEXSTACK_PLATFORM_DELIVERABLES_DIR}/VST3"
            VST_COPY_DIR "${HEXSTACK_PLATFORM_DELIVERABLES_DIR}/VST2")
    elseif(APPLE)
        list(APPEND HEXSTACK_PLUGIN_COPY_ARGS
            VST3_COPY_DIR "${HEXSTACK_PLATFORM_DELIVERABLES_DIR}/VST3"
            VST_COPY_DIR "${HEXSTACK_PLATFORM_DELIVERABLES_DIR}/VST2"
            AU_COPY_DIR "${HEXSTACK_PLATFORM_DELIVERABLES_DIR}/AU")
    endif()
endif()

if(APPLE)
    list(APPEND HEXSTACK_PLUGIN_FORMATS AU)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target" FORCE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures (universal binary)" FORCE)
endif()

if(HEXSTACK_ENABLE_VST2)
    if(NOT HEXSTACK_VST2_SDK_PATH)
        message(FATAL_ERROR "HEXSTACK_ENABLE_VST2=ON requires HEXSTACK_VST2_SDK_PATH to point to a legacy VST2 SDK.")
    endif()

    set(HEXSTACK_VST2_AEFFECT_CANDIDATES
        "${HEXSTACK_VST2_SDK_PATH}/pluginterfaces/vst2.x/aeffect.h"
        "${HEXSTACK_VST2_SDK_PATH}/public.sdk/source/vst2.x/aeffect.h")

    set(HEXSTACK_VST2_SDK_LAYOUT_OK FALSE)
    foreach(vst2_header ${HEXSTACK_VST2_AEFFECT_CANDIDATES})
        if(EXISTS "${vst2_header}")
            set(HEXSTACK_VST2_SDK_LAYOUT_OK TRUE)
            break()
        endif()
    endforeach()

    if(NOT HEXSTACK_VST2_SDK_LAYOUT_OK)
        message(FATAL_ERROR
            "HEXSTACK_VST2_SDK_PATH does not appear to contain a legacy VST2 SDK. "
            "Expected aeffect.h under pluginterfaces/vst2.x or public.sdk/source/vst2.x.")
    endif()

    juce_set_vst2_sdk_path("${HEXSTACK_VST2_SDK_PATH}")
    list(APPEND HEXSTACK_PLUGIN_FORMATS VST)
endif()

juce_add_plugin(Hexstack
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "INFERNO TONES"
    BUNDLE_ID com.infernotones.hexstack
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE IfAu
    PLUGIN_CODE HxSk
    FORMATS ${HEXSTACK_PLUGIN_FORMATS}
    PRODUCT_NAME "HEXSTACK"
    ICON_BIG "${CMAKE_SOURCE_DIR}/AMP.png"
    ICON_SMALL "${CMAKE_SOURCE_DIR}/AMP.png"
    ${HEXSTACK_PLUGIN_COPY_ARGS}
)

# On macOS, embed the .icns so the .app shows the correct icon in Finder/Dock
if(APPLE AND DEFINED HEXSTACK_MACOS_ICNS AND EXISTS "${HEXSTACK_MACOS_ICNS}")
    set_target_properties(Hexstack_Standalone PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "AMP.icns"
    )
    target_sources(Hexstack_Standalone PRIVATE "${HEXSTACK_MACOS_ICNS}")
    set_source_files_properties("${HEXSTACK_MACOS_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

juce_generate_juce_header(Hexstack)

juce_add_binary_data(HexstackAssets
    SOURCES
        AMP.png
        BACKGROUND.jpeg
        TITLE.png
        "pedal stomp.png"
        "EXP PEDAL RED.png"
        "EXP PEDAL BLACK.png"
)

target_sources(Hexstack
    PRIVATE
        source/PluginProcessor.cpp
        source/PluginProcessor.h
        source/PluginEditor.cpp
        source/PluginEditor.h
)

target_include_directories(Hexstack PRIVATE source)

target_compile_definitions(Hexstack
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/ASIOSDK/common")
    target_compile_definitions(Hexstack PUBLIC JUCE_ASIO=1)
    target_include_directories(Hexstack PRIVATE
        "${CMAKE_SOURCE_DIR}/ASIOSDK/common"
        "${CMAKE_SOURCE_DIR}/ASIOSDK/host/pc"
    )
    message(STATUS "ASIO SDK found — ASIO support enabled")
endif()

target_link_libraries(Hexstack
    PRIVATE
        HexstackAssets
        juce::juce_audio_utils
        juce::juce_dsp
        signalsmith-stretch
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(TARGET Hexstack_Standalone)
    add_custom_command(TARGET Hexstack_Standalone POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${HEXSTACK_PLATFORM_DELIVERABLES_DIR}/Standalone"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Hexstack_Standalone>"
                "${HEXSTACK_PLATFORM_DELIVERABLES_DIR}/Standalone/$<TARGET_FILE_NAME:Hexstack_Standalone>"
        VERBATIM)
endif()
