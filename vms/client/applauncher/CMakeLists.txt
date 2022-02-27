## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(LINUX)
    list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/../lib/stdcpp")
endif()

nx_add_target(applauncher EXECUTABLE SIGNED
    RC_FILE
        "applauncher.rc"
    PUBLIC_LIBS
        qtsinglecoreapplication
        nx_vms_applauncher_api
        nx_zip
        nx_kit
    FOLDER utils
)

if(LINUX)
    target_link_libraries(applauncher PRIVATE pthread)
endif()

string(REPLACE ".exe" "" binary_name ${applauncher.binary.name})
set_target_properties(applauncher PROPERTIES OUTPUT_NAME ${binary_name})
