set(customWebAdminVersion "" CACHE STRING
    "Custom version for server-external package")
mark_as_advanced(customWebAdminVersion)
set(customWebAdminPackageDirectory "" CACHE STRING
    "Custom location of server-external package")
mark_as_advanced(customWebAdminPackageDirectory)

function(_nx_create_vms_configuration)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(prefer_debug_packages "True")
    else()
        set(prefer_debug_packages "False")
    endif()

    set(content "def cmake_to_bool(value): return value.lower() in ('on', 'true')\n\n")
    string(APPEND content "PACKAGES_DIR = '${PACKAGES_DIR}'\n")
    string(APPEND content "rdepSync = cmake_to_bool('${rdepSync}')\n")
    string(APPEND content "prefer_debug_packages = ${prefer_debug_packages}\n")
    string(APPEND content "cmake_include_file = '${CMAKE_BINARY_DIR}/dependencies.cmake'\n")
    string(APPEND content "platform = '${platform}'\n")
    string(APPEND content "arch = '${arch}'\n")
    string(APPEND content "rdep_target = '${rdep_target}'\n")
    string(APPEND content "box = '${box}'\n")
    string(APPEND content "branch = '${branch}'\n")
    string(APPEND content "customization = '${customization}'\n")
    string(APPEND content "releaseVersion = '${releaseVersion}'\n")
    string(APPEND content "shortReleaseVersion = '${releaseVersion.short}'\n")
    string(APPEND content "customWebAdminVersion = '${customWebAdminVersion}'\n")
    string(APPEND content "customWebAdminPackageDirectory = '${customWebAdminPackageDirectory}'\n")
    string(APPEND content "withMediaServer = cmake_to_bool('${withMediaServer}')\n")
    string(APPEND content "withTrayTool = cmake_to_bool('${withTrayTool}')\n")
    string(APPEND content "withNxTool = cmake_to_bool('${withNxTool}')\n")
    string(APPEND content "withDesktopClient = cmake_to_bool('${withDesktopClient}')\n")
    string(APPEND content "withMobileClient = cmake_to_bool('${withMobileClient}')\n")
    string(APPEND content "withClouds = cmake_to_bool('${withClouds}')\n")
    string(APPEND content "withTestCamera = cmake_to_bool('${withTestCamera}')\n")
    string(APPEND content "withTests = cmake_to_bool('${withTests}')\n")

    file(WRITE ${CMAKE_BINARY_DIR}/vms_configuration.py ${content})
    nx_store_known_file(${CMAKE_BINARY_DIR}/vms_configuration.py)
endfunction()

_nx_create_vms_configuration()

if(WIN32)
    set(_sep ";")
else()
    set(_sep ":")
endif()

set(ENV{PYTHONPATH} "${CMAKE_SOURCE_DIR}/build_utils/python${_sep}${CMAKE_BINARY_DIR}")
execute_process(
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/sync_dependencies.py
    RESULT_VARIABLE sync_result
)

if(NOT sync_result STREQUAL "0")
    message(FATAL_ERROR "error: Packages sync failed")
endif()

if(WIN32)
    set(nxKitLibraryType "SHARED" CACHE STRING "" FORCE)
endif()

if(customWebAdminPackageDirectory)
    nx_copy_package(${customWebAdminPackageDirectory})
endif()

include(${CMAKE_BINARY_DIR}/dependencies.cmake)

foreach(package_dir ${synched_package_dirs})
    if(NOT EXISTS ${package_dir}/.nocopy)
        nx_copy_package(${package_dir})
    endif()
endforeach()

file(TO_CMAKE_PATH "${QT_DIR}" QT_DIR)

function(copy_linux_cpp_runtime)
    execute_process(COMMAND
        ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build_utils/linux/copy_system_library.py
            --compiler ${CMAKE_CXX_COMPILER}
            --flags "${CMAKE_CXX_FLAGS}"
            --dest-dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
            --list
            ${cpp_runtime_libs}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot copy C++ runtime libraries.")
    endif()

    nx_split_string(files "${output}")
    nx_store_known_files(${files})
endfunction()

if(LINUX)
    set(cpp_runtime_libs libstdc++.so.6 libatomic.so.1 libgcc_s.so.1)

    if(arch MATCHES "x64|x86")
        if(arch STREQUAL "x64")
            list(APPEND cpp_runtime_libs libmvec.so.1)
        endif()

        copy_linux_cpp_runtime()
    endif()

    string(REPLACE ";" " " cpp_runtime_libs_string "${cpp_runtime_libs}")
endif()
