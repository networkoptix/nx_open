## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_program(doxygen_executable doxygen HINTS ${CONAN_DOXYGEN_ROOT}/bin NO_DEFAULT_PATH)

if(NOT doxygen_executable)
    message(FATAL_ERROR "Cannot find doxygen.")
endif()
