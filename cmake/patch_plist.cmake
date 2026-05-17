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

# Make Finder show a clear "requires macOS 11" message on older machines
# (the binary is already compiled with -mmacosx-version-min=11.0).
execute_process(
    COMMAND /usr/libexec/PlistBuddy -c "Delete :LSMinimumSystemVersion" "${PLIST}"
    OUTPUT_QUIET ERROR_QUIET
)
execute_process(
    COMMAND /usr/libexec/PlistBuddy
            -c "Add :LSMinimumSystemVersion string 11.0"
            "${PLIST}"
    RESULT_VARIABLE lsmin_result
)
if(NOT lsmin_result EQUAL 0)
    message(FATAL_ERROR "PlistBuddy: failed to set LSMinimumSystemVersion in ${PLIST}")
endif()
