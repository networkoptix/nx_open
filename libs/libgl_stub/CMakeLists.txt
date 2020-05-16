set_output_directories(LIBRARY lib/libgl_stub)

set(CMAKE_C_VISIBILITY_PRESET default)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

add_library(libgl_stub src/gl_dummy.c)
set_target_properties(libgl_stub PROPERTIES
    OUTPUT_NAME GL
    SOVERSION 1
)
