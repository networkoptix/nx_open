## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(ANDROID OR IOS OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
else()
    set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")
endif()

link_directories("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

if(LINUX)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${QT_DIR}/lib")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath-link,${QT_DIR}/lib")
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()
