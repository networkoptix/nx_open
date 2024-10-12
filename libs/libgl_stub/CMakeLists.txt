## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set_output_directories(LIBRARY lib/libgl_stub)

set(CMAKE_C_VISIBILITY_PRESET default)
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

add_library(libgl_stub src/gl_dummy.c)
set_target_properties(libgl_stub PROPERTIES
    OUTPUT_NAME OpenGL
    SOVERSION 0
)

add_library(libglx_stub src/glx_dummy.c)
set_target_properties(libglx_stub PROPERTIES
    OUTPUT_NAME GLX
    SOVERSION 0
)

add_library(libegl_stub src/egl_dummy.c)
set_target_properties(libegl_stub PROPERTIES
    OUTPUT_NAME EGL
    SOVERSION 1
)

add_library(libx11_stub src/x11_dummy.c)
set_target_properties(libx11_stub PROPERTIES
    OUTPUT_NAME X11
    SOVERSION 6
)

add_library(libva-x11_stub src/va-x11_dummy.c)
set_target_properties(libva-x11_stub PROPERTIES
    OUTPUT_NAME va-x11
    SOVERSION 2
)
