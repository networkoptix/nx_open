nx_add_target(mediaserver EXECUTABLE
    PUBLIC_LIBS mediaserver_core)

if(TARGET festival_libs)
    target_link_libraries(mediaserver PRIVATE festival_libs)
endif()

# Security requirement (see hardening-check).
string(APPEND CMAKE_EXE_LINKER_FLAGS " -pie")

