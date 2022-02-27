## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_library(nx_preprocessor INTERFACE)
target_include_directories(nx_preprocessor INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(withTests)
    add_subdirectory(unit_tests)
endif()
