nx_add_target(desktop_client EXECUTABLE SIGNED
    RC_FILE
        "desktop_client.rc"
    NO_MOC
    NO_PCH
    PRIVATE_LIBS
        nx_vms_client_desktop /usr/local/lib/libva-x11.so)

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
