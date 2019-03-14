nx_add_target(root_tool EXECUTABLE
    PUBLIC_LIBS boost nx_system_commands pthread)

if(LINUX)
    add_definitions(-DNX_USER_NAME=${deb.customization.company.name})
    # Security requirement (see hardening-check).
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")
    # Root Tool uses setuid flag which does not allow to use any library paths but ld.conf.
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -static-libstdc++")
endif()
