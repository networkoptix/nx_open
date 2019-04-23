set(rdepOverrides "" CACHE STRING "RDep package version or location overrides")
mark_as_advanced(rdepOverrides)

# TODO: Remove this variable because its goal can be achieved with rdepOverrides.
set(customWebAdminPackageDirectory "" CACHE STRING
    "Custom location of server-external package")
mark_as_advanced(customWebAdminPackageDirectory)

set(cmake_include_file ${CMAKE_BINARY_DIR}/dependencies.cmake)

set(sync_target ${targetDevice})
if(NOT sync_target)
    set(sync_target ${default_target_device})
endif()

set(sync_command ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/sync_dependencies.py
    --packages-dir=${PACKAGES_DIR}
    --target=${rdep_target}
    --release-version=${releaseVersion}
    --customization=${customization}
    --cmake-include-file=${cmake_include_file}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND sync_command "--debug")
endif()

if(customWebAdminPackageDirectory)
    list(INSERT rdepOverrides 0 "server-external=${customWebAdminPackageDirectory}")
else()
    list(INSERT rdepOverrides 0 "server-external=${branch}")
endif()

if(rdepOverrides)
    list(APPEND sync_command --overrides ${rdepOverrides})
endif()

set(sync_options)

if(targetDevice MATCHES "-clang")
    list(APPEND sync_options clang)
endif()

if(sync_options)
    list(APPEND sync_command --options ${sync_options})
endif()

set(ENV{PYTHONPATH} "${CMAKE_SOURCE_DIR}/build_utils/python")
execute_process(COMMAND ${sync_command} RESULT_VARIABLE sync_result)

if(NOT sync_result STREQUAL "0")
    message(FATAL_ERROR "error: Packages sync failed")
endif()

function(copy_system_libraries)
    execute_process(COMMAND
        ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build_utils/linux/copy_system_library.py
            --compiler ${CMAKE_CXX_COMPILER}
            --flags "${CMAKE_CXX_FLAGS}"
            --link-flags "${CMAKE_SHARED_LINKER_FLAGS}"
            --dest-dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
            --list
            ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot copy system libraries: ${ARGN}.")
    endif()

    nx_split_string(files "${output}")
    nx_store_known_files(${files})
endfunction()

macro(load_dependencies)
    if(WIN32)
        set(nxKitLibraryType "SHARED" CACHE STRING "" FORCE)
    endif()

    include(${cmake_include_file})

    foreach(package_dir ${synched_package_dirs})
        if(EXISTS ${package_dir}/.nocopy)
            continue()
        endif()

        if(targetDevice STREQUAL "linux_arm32" AND package_dir MATCHES "ffmpeg-arm32")
            nx_copy_package_separately(${package_dir} "ffmpeg-arm32")
            continue()
        endif()

        if(targetDevice STREQUAL "linux_arm32" AND package_dir MATCHES "ffmpeg-rpi")
            nx_copy_package_separately(${package_dir} "ffmpeg-rpi")
            continue()
        endif()

        nx_copy_package(${package_dir})
    endforeach()

    file(TO_CMAKE_PATH "${QT_DIR}" QT_DIR)

    if(LINUX)
        set(cpp_runtime_libs libstdc++.so.6 libatomic.so.1 libgcc_s.so.1)
        set(icu_runtime_libs libicuuc.so.55 libicudata.so.55 libicui18n.so.55)

        if(arch MATCHES "x64|x86")
            if(arch STREQUAL "x64")
                list(APPEND cpp_runtime_libs libmvec.so.1)
            endif()

            copy_system_libraries(${cpp_runtime_libs})
            copy_system_libraries(${icu_runtime_libs})
            nx_copy_package(${QT_DIR})
        endif()

        string(REPLACE ";" " " cpp_runtime_libs_string "${cpp_runtime_libs}")
        string(REPLACE ";" " " icu_runtime_libs_string "${icu_runtime_libs}")
    endif()

    if(ANDROID)
        if(NOT android_sdk_directory)
            set(android_sdk_directory $ENV{ANDROID_HOME})
        endif()
    endif()
endmacro()
