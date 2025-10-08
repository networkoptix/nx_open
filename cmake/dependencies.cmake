## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(LINUX AND NOT ANDROID)
    if(targetDevice STREQUAL "linux_x64" OR targetDevice STREQUAL "linux_arm64")
        set(OS_DEPS_ROOT ${CONAN_OS_DEPS_FOR_DESKTOP_LINUX_ROOT})
    else()
        set(OS_DEPS_ROOT ${CONAN_OS_DEPS_FOR_LINUX_DEVICES_ROOT})
    endif()

    include(${OS_DEPS_ROOT}/os_deps.cmake)
    set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
    set(os_deps_pkg_config_dir ${CMAKE_BINARY_DIR}/os_deps_pkg_config)
    nx_store_known_files_in_directory(${os_deps_pkg_config_dir})
    set(ENV{PKG_CONFIG_PATH} ${os_deps_pkg_config_dir})

    set(cpp_runtime_libs libstdc++.so.6 libgcc_s.so.1)
    if(arch STREQUAL "x64")
        list(APPEND cpp_runtime_libs libmvec.so.1)
    endif()

    nx_detect_default_use_system_stdcpp_value(${developerBuild})

    if(NOT useSystemStdcpp)
        nx_copy_system_libraries(${cpp_runtime_libs})
    endif()

    string(REPLACE ";" " " cpp_runtime_libs_string "${cpp_runtime_libs}")

    # Although libatomic is a part or the C++ runtime, we do not include it into cpp_runtime_libs.
    # We handle it separately and for now we just copy it into the final package `lib`.
    nx_copy_system_libraries(libatomic.so.1)
endif()
