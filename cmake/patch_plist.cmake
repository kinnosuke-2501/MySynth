# Invoked by CMake POST_BUILD: add/update NSMidiUsageDescription in Info.plist.
# Usage: cmake -DPLIST=<path/to/Info.plist> -P patch_plist.cmake

execute_process(
    COMMAND /usr/libexec/PlistBuddy -c "Delete :NSMidiUsageDescription" "${PLIST}"
    OUTPUT_QUIET ERROR_QUIET
)
execute_process(
    COMMAND /usr/libexec/PlistBuddy
            -c "Add :NSMidiUsageDescription string 'This app uses MIDI input to receive notes.'"
            "${PLIST}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "PlistBuddy: failed to set NSMidiUsageDescription in ${PLIST}")
endif()
