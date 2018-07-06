find_program(doxygen_executable doxygen HINTS ${doxygen_directory}/bin NO_DEFAULT_PATH)

if(NOT doxygen_executable)
    message(FATAL_ERROR "Cannot find doxygen.")
endif()
