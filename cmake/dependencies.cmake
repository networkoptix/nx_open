## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(LINUX AND NOT useCurrentOsComponents AND NOT ANDROID)
    if(targetDevice STREQUAL "linux_x64" OR targetDevice STREQUAL "linux_arm64")
        set(OS_DEPS_ROOT ${CONAN_OS_DEPS_FOR_DESKTOP_LINUX_ROOT})
    else()
        set(OS_DEPS_ROOT ${CONAN_OS_DEPS_FOR_LINUX_DEVICES_ROOT})
    endif()

    include(${OS_DEPS_ROOT}/os_deps.cmake)

    if(arch STREQUAL "arm")
        set(icu_version 57)
    else()
        set(icu_version 60)
    endif()

    set(icu_runtime_libs
        libicuuc.so.${icu_version}
        libicudata.so.${icu_version}
        libicui18n.so.${icu_version})

    nx_copy_system_libraries(${icu_runtime_libs})

    set(cpp_runtime_libs libstdc++.so.6 libatomic.so.1 libgcc_s.so.1)
    if(arch STREQUAL "x64")
        list(APPEND cpp_runtime_libs libmvec.so.1)
    endif()

    nx_detect_default_use_system_stdcpp_value(${developerBuild})

    if(NOT useSystemStdcpp)
        nx_copy_system_libraries(${cpp_runtime_libs})
    endif()

    string(REPLACE ";" " " cpp_runtime_libs_string "${cpp_runtime_libs}")
    string(REPLACE ";" " " icu_runtime_libs_string "${icu_runtime_libs}")
endif()
