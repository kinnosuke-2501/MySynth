cmake_minimum_required(VERSION 3.22)

# --- Distribution settings (must be set before project()) ----------------
# Universal binary so the build runs on both Apple Silicon and Intel Macs,
# and a conservative minimum OS so older machines can run it.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(MySynth VERSION 1.0.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(JUCE)

# Product identity. Internal target stays "MySynth" (keeps build paths and
# the ~/Library/Application Support/MySynth settings folder stable); only the
# user-facing name/ids change. Rename freely here.
juce_add_gui_app(MySynth
    PRODUCT_NAME     "StageFM"
    COMPANY_NAME     "StageFM"
    BUNDLE_ID        "com.stagefm.app"
    VERSION          "1.0.0"
    ICON_BIG         "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon_1024.png"
    NEEDS_MIDI_INPUT TRUE
)

juce_generate_juce_header(MySynth)

target_sources(MySynth PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/SynthVoice.cpp
    Source/VintageLookAndFeel.cpp
)

target_compile_features(MySynth PRIVATE cxx_std_17)

target_link_libraries(MySynth PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# macOS 12+ で MIDI アクセスに必要な usage description を Info.plist に追記
if(APPLE)
    add_custom_command(TARGET MySynth POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DPLIST=$<TARGET_BUNDLE_CONTENT_DIR:MySynth>/Info.plist
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch_plist.cmake
        COMMENT "Patching Info.plist: NSMidiUsageDescription"
    )
endif()
