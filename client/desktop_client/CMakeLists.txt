nx_add_target(desktop_client EXECUTABLE SIGNED
    RC_FILE
        "desktop_client.rc"
    PUBLIC_LIBS
        qtsingleapplication nx_kit nx_client_desktop)

if(WINDOWS)
    target_link_libraries(desktop_client PUBLIC nx_gdi_detours)
endif()

if(LINUX)
    # Security requirement (see hardening-check).
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")
endif()

string(REPLACE ".exe" "" binary_name ${client.binary.name})
set_target_properties(desktop_client PROPERTIES OUTPUT_NAME ${binary_name})
