nx_add_target(root_tool EXECUTABLE
    PUBLIC_LIBS boost root_tool_lib)

if(LINUX)
    # Security requirement (see hardening-check).
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")
    # Root Tool Library uses setuid flag which does not allow to use any library paths but ld.conf.
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -static-libstdc++")
endif()
