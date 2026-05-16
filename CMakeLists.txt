cmake_minimum_required(VERSION 3.22)
project(MySynth VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(JUCE)

juce_add_gui_app(MySynth
    PRODUCT_NAME     "MySynth"
    COMPANY_NAME     "MySynthDev"
    BUNDLE_ID        "com.mysynthdev.mysynth"
    NEEDS_MIDI_INPUT TRUE
)

juce_generate_juce_header(MySynth)

target_sources(MySynth PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/SynthVoice.cpp
)

target_compile_features(MySynth PRIVATE cxx_std_17)

target_link_libraries(MySynth PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_utils
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
