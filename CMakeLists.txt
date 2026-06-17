cmake_minimum_required(VERSION 3.22)

project(VoxReTune VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#include(FetchContent)
#FetchContent_Declare(
#    JUCE
#    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
#    GIT_TAG        8.0.6
#    GIT_SHALLOW    TRUE
#)
#FetchContent_MakeAvailable(JUCE)

# ── JUCE (override: cmake -B build -DJUCE_PATH=/path/to/JUCE) ──────────────────
if(WIN32)
    set(_juce_path_default "C:/JUCE")
else()
    set(_juce_path_default "$ENV{HOME}/JUCE")
endif()
set(JUCE_PATH "${_juce_path_default}" CACHE PATH "Path to JUCE root (clone or SDK)")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at:\n  ${JUCE_PATH}\n"
            "Set JUCE_PATH when configuring, e.g.\n"
            "  cmake -B build -DJUCE_PATH=/path/to/JUCE")
endif()

add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")


juce_add_plugin(VoxReTune
    COMPANY_NAME "Studio9Plugins"
    BUNDLE_ID com.voxretune.plugin
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE S9Pl
    PLUGIN_CODE Vrt1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "VoxReTune"
    # VST3 category
    VST3_CATEGORIES         "Fx" 

    # Version
    VERSION                 1.0.0)

target_sources(VoxReTune
    PRIVATE
        source/plugin/PluginProcessor.cpp
        source/plugin/PluginEditor.cpp
        source/detection/YinPitchDetector.cpp
        source/engine/CorrectionEngine.cpp
        source/ui/PianoRollComponent.cpp)

target_include_directories(VoxReTune
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source)

target_compile_definitions(VoxReTune
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VoxReTune
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ── Auto-install VST3 after build ─────────────────────────────────────────────
option(COPY_PLUGIN_AFTER_BUILD "Copy VST3 to system VST3 folder after build" ON)

if(COPY_PLUGIN_AFTER_BUILD)

    if(UNIX AND NOT APPLE)
        # Linux — installs to ~/.vst3/VoxReTune.vst3
        set(VST3_INSTALL_DIR "$ENV{HOME}/.vst3")

        add_custom_command(TARGET VoxReTune_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_INSTALL_DIR}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${VST3_INSTALL_DIR}/VoxReTune.vst3"
            # TARGET_FILE_DIR = …/VoxReTune.vst3/Contents/<arch>-os/ — two levels up = bundle root (not three; three copies the folder that *contains* .vst3 and nests an extra VoxReTune.vst3).
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:VoxReTune_VST3>/../.."
                "${VST3_INSTALL_DIR}/VoxReTune.vst3"
            COMMENT "-- Installing VoxReTune.vst3 → ${VST3_INSTALL_DIR}/VoxReTune.vst3"
        )

    elseif(WIN32)
        # Windows — installs to Common Files\VST3 (may require elevated build)
        set(VST3_INSTALL_DIR "$ENV{COMMONPROGRAMFILES}/VST3")
        # Alternative Windows path — no Admin needed
        #set(VST3_INSTALL_DIR "$ENV{LOCALAPPDATA}/Programs/Common/VST3")

        add_custom_command(TARGET VocReTuneOne_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_INSTALL_DIR}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${VST3_INSTALL_DIR}/VoxReTune.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:VoxReTune_VST3>/../.."
                "${VST3_INSTALL_DIR}/VoxReTune.vst3"
            COMMENT "-- Installing VoxReTune.vst3 → ${VST3_INSTALL_DIR}"
        )

    elseif(APPLE)
        # macOS — installs to ~/Library/Audio/Plug-Ins/VST3/VoxReTune.vst3
        set(VST3_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")

        add_custom_command(TARGET VoxReTune_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${VST3_INSTALL_DIR}"
            COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${VST3_INSTALL_DIR}/VoxReTune.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:VoxReTune_VST3>/../.."
                "${VST3_INSTALL_DIR}/VoxReTune.vst3"
            COMMENT "-- Installing VoxReTune.vst3 → ${VST3_INSTALL_DIR}"
        )

    endif()

endif()