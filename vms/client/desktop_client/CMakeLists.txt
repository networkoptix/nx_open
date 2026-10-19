## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(MACOSX)
    set(mac_sources src/mac_bundle_launcher.mm)
endif()

nx_add_target(desktop_client EXECUTABLE SIGNED
    RC_FILE
        "desktop_client.rc"
    ADDITIONAL_SOURCES
        ${mac_sources}
    NO_MOC
    NO_PCH
    ROOT_FOLDER
    PRIVATE_LIBS
        nx_vms_client_desktop)

if(LINUX)
    # Security requirement (see hardening-check).
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")
endif()

if(LINUX)
    set(binary_name ${client.binary.name})
else()
    string(REPLACE ".exe" "" binary_name ${client.binary.name})
endif()

set_target_properties(desktop_client PROPERTIES OUTPUT_NAME ${binary_name})
