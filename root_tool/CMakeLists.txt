nx_add_target(root_tool EXECUTABLE NO_PCH
    PUBLIC_LIBS boost)

if(LINUX)
    # Security requirement (see hardening-check).
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")

    # Root Tool uses setuid flag which does not allow to use any library paths but ld.conf.
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -static-libstdc++")
endif()

