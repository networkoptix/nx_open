## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(ANDROID OR IOS OR EMSCRIPTEN)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
else()
    set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")
endif()

link_directories("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

if(WINDOWS)
    set(buildStepTimeoutSeconds 1800 CACHE STRING
        "Linking a target or generating its debug symbols is killed after this many seconds.")

    # A hung linker or symbol dumper would otherwise block the build until the CI job times out,
    # without telling which process got stuck.
    set(nx_run_with_timeout
        "${PYTHON_EXECUTABLE}" "${open_build_utils_dir}/python/run_with_timeout.py"
        --timeout ${buildStepTimeoutSeconds} --dump-dir "${CMAKE_BINARY_DIR}/build_logs" --)
    set(CMAKE_C_LINKER_LAUNCHER ${nx_run_with_timeout})
    set(CMAKE_CXX_LINKER_LAUNCHER ${nx_run_with_timeout})
endif()

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
