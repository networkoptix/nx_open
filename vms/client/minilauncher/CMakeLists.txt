## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_compile_options(/MT)

nx_configure_file(version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

nx_add_target(minilauncher EXECUTABLE SIGNED
    RC_FILE
        "minilauncher.rc"
    PUBLIC_LIBS
        shlwapi.lib
    FOLDER utils
)

string(REPLACE ".exe" "" binary_name ${minilauncher.binary.name})
set_target_properties(minilauncher PROPERTIES
    OUTPUT_NAME ${binary_name}
    WIN32_EXECUTABLE ON
)

target_include_directories(minilauncher PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
