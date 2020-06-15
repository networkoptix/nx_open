set(rdepOverrides "" CACHE STRING "RDep package version or location overrides")
option(rdepForce "Force RDep to run rsync even if package timestamp is up to date" OFF)
option(rdepVerbose "Enable RDep verbose output" OFF)
mark_as_advanced(rdepOverrides rdepForce rdepVerbose)

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

if(NOT rdepSync)
    list(APPEND sync_command "--use-local")
endif()

if(rdepForce)
    list(APPEND sync_command "--force")
endif()

if(rdepVerbose)
    list(APPEND sync_command "--verbose")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND sync_command "--debug")
endif()

if(customWebAdminPackageDirectory)
    list(INSERT rdepOverrides 0 "server-external=${customWebAdminPackageDirectory}")
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
            --compiler=${CMAKE_CXX_COMPILER}
            --flags=${CMAKE_CXX_FLAGS}
            --link-flags=${CMAKE_SHARED_LINKER_FLAGS}
            --dest-dir=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
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

macro(load_generated_dependencies_file)
    include(${cmake_include_file})
endmacro()

macro(load_dependencies)
    if(WIN32)
        set(nxKitLibraryType "SHARED" CACHE STRING "" FORCE)
    endif()

    foreach(package_dir ${synched_package_dirs})
        if(EXISTS ${package_dir}/.nocopy)
            continue()
        endif()

        if(targetDevice STREQUAL "linux_arm32" AND package_dir MATCHES "ffmpeg")
            if(package_dir MATCHES "rpi")
                nx_copy_package_separately(${package_dir} "ffmpeg-rpi")
            else()
                nx_copy_package_separately(${package_dir} "ffmpeg-arm32")
            endif()

            set(ffmpeg_link ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/ffmpeg)
            if(NOT EXISTS ${ffmpeg_link})
                # If you need a symlink to RPI variant, re-create it manually in the libs dir.
                # CMake won't overwrite it.
                file(CREATE_LINK "ffmpeg-arm32" ${ffmpeg_link} SYMBOLIC)
                nx_store_known_file(${ffmpeg_link})
            endif()

            continue()
        endif()

        nx_copy_package(${package_dir})
    endforeach()

    file(TO_CMAKE_PATH "${QT_DIR}" QT_DIR)

    if(LINUX)
        set(cpp_runtime_libs libstdc++.so.6 libatomic.so.1 libgcc_s.so.1)
        set(icu_runtime_libs libicuuc.so.55 libicudata.so.55 libicui18n.so.55)

        if(arch STREQUAL "x64")
            list(APPEND cpp_runtime_libs libmvec.so.1)
        endif()

        if(arch MATCHES "x64|x86|arm64")
            copy_system_libraries(${icu_runtime_libs})
        endif()

        nx_copy_package(${QT_DIR} SKIP_BIN)

        # Check if we need newer libstdc++.
        if(NOT DEFINED CACHE{useSystemStdcpp})
            set(useSystemStdcpp OFF)

            if(developerBuild AND arch MATCHES "x64|x86")
                execute_process(
                    COMMAND ${CMAKE_CXX_COMPILER} --print-file-name libstdc++.so.6
                    OUTPUT_VARIABLE stdcpp_lib_path
                )

                if(stdcpp_lib_path)
                    get_filename_component(stdcpp_dir ${stdcpp_lib_path} DIRECTORY)
                    execute_process(
                        COMMAND ${CMAKE_SOURCE_DIR}/build_utils/linux/choose_newer_stdcpp.sh
                            ${stdcpp_dir}
                        OUTPUT_VARIABLE selected_stdcpp
                    )
                else()
                    set(selected_stdcpp)
                endif()

                if(NOT selected_stdcpp)
                    set(useSystemStdcpp ON)
                endif()
            endif()

            set(useSystemStdcpp ${useSystemStdcpp} CACHE BOOL
                "Use system libstdc++ and do not copy it to lib directory from the compiler.")
        endif()

        if(NOT useSystemStdcpp)
            copy_system_libraries(${cpp_runtime_libs})
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
