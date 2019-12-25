if(LINUX)
    # In Linux root tool has setuid flag which does not allow usage of $ORIGIN. Such executables
    # should use absolute paths. "$ORIGIN/../lib" is still there for development reasons.
    list(APPEND CMAKE_INSTALL_RPATH "/opt/${customization.companyId}/mediaserver/lib")
endif()

nx_add_target(root_tool EXECUTABLE
    PUBLIC_LIBS boost nx_system_commands pthread
    FOLDER utils)

if(LINUX)
    add_definitions(-DNX_USER_NAME=${customization.companyId})
    # Security requirement (see hardening-check).
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")
    # Root Tool uses setuid flag which does not allow to use any library paths but ld.conf.
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -static-libstdc++")
endif()
