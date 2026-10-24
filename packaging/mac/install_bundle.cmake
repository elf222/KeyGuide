# Replaces DEST with the signed and verified STAGING bundle (run by package-macos):
#   cmake -DSTAGING=<build>/package-macos/KeyGuide.app -DDEST=/Applications/KeyGuide.app -P ...
# The previous DEST is only removed once the new copy is complete, so a failed copy never
# leaves the user without an app.
if (NOT STAGING OR NOT DEST OR NOT IS_DIRECTORY "${STAGING}")
  message(FATAL_ERROR "install_bundle.cmake needs -DSTAGING=<bundle> and -DDEST=<bundle>")
endif()
file(REMOVE_RECURSE "${DEST}.new" "${DEST}.old")
execute_process(COMMAND ditto "${STAGING}" "${DEST}.new" RESULT_VARIABLE copy_result)
if (NOT copy_result EQUAL 0)
  file(REMOVE_RECURSE "${DEST}.new")
  message(FATAL_ERROR "Could not copy KeyGuide.app to ${DEST}; the installed copy is unchanged")
endif()
if (EXISTS "${DEST}")
  file(RENAME "${DEST}" "${DEST}.old")
endif()
file(RENAME "${DEST}.new" "${DEST}")
file(REMOVE_RECURSE "${DEST}.old" "${STAGING}")
message(STATUS "KeyGuide.app: ${DEST}")
