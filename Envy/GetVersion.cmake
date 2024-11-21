find_package(Git)

if(GIT_EXECUTABLE)
  get_filename_component(WORKING_DIR ${SRC} DIRECTORY)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${WORKING_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    RESULT_VARIABLE ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if(GIT_VERSION STREQUAL "")
  set(GIT_VERSION release)
  message(WARNING "Failed to determine version from Git tags. Using default version \"${GIT_VERSION}\".")
endif()

configure_file(${SRC} ${DST} @ONLY)