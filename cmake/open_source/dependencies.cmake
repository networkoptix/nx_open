## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# This file is specific for open directory and must not be used in the main project.

if(LINUX)
    set(OS_DEPS_ROOT ${CONAN_OS_DEPS_FOR_DESKTOP_LINUX_ROOT})
    include(${OS_DEPS_ROOT}/os_deps.cmake)

    set(addition_rpaths "")
    string(APPEND addition_rpaths " -Wl,-rpath,${QT_DIR}/lib")

    string(APPEND CMAKE_EXE_LINKER_FLAGS "${addition_rpaths}")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS "${addition_rpaths}")

    if(arch STREQUAL "arm")
        set(icu_version 52)
    else()
        set(icu_version 55)
    endif()

    set(icu_runtime_libs
        libicuuc.so.${icu_version}
        libicudata.so.${icu_version}
        libicui18n.so.${icu_version})

    nx_copy_system_libraries(${icu_runtime_libs})

    nx_detect_default_use_system_stdcpp_value(${developerBuild})

    if(NOT useSystemStdcpp)
        set(cpp_runtime_libs libstdc++.so.6 libatomic.so.1 libgcc_s.so.1)
        if(arch STREQUAL "x64")
            list(APPEND cpp_runtime_libs libmvec.so.1)
        endif()

        nx_copy_system_libraries(${cpp_runtime_libs})
    endif()

    string(REPLACE ";" " " cpp_runtime_libs_string "${cpp_runtime_libs}")
    string(REPLACE ";" " " icu_runtime_libs_string "${icu_runtime_libs}")
endif()
