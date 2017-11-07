nx_add_target(desktop_client EXECUTABLE
    PUBLIC_LIBS qtsingleapplication nx_kit nx_client_desktop)

if(LINUX)
    # Security requirement (see hardening-check).
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")
endif()
