if(NOT rdep_dir)
    set(rdep_dir "${CMAKE_SOURCE_DIR}/build_utils/python")
endif()

set(packagesDir "" CACHE STRING "Custom RDep repository directory")
mark_as_advanced(packagesDir)

if(packagesDir)
    set(PACKAGES_DIR ${packagesDir})
else()
    set(PACKAGES_DIR $ENV{RDEP_PACKAGES_DIR})
    if(NOT PACKAGES_DIR)
        # TODO: This block is for backward compatibility. Remove it when CI is re-configured.
        set(PACKAGES_DIR $ENV{environment})
        if(PACKAGES_DIR)
            string(APPEND PACKAGES_DIR /packages)
        endif()
    endif()
endif()

if(NOT PACKAGES_DIR)
    set(message "RDep packages dir is not specified. Please provide one of:")
    string(APPEND message "\n  RDEP_PACKAGES_DIR environment variable")
    string(APPEND message "\n  packagesDir CMake cache variable (cmake -DpackagesDir=...)")
    message(FATAL_ERROR ${message})
endif()

file(TO_CMAKE_PATH ${PACKAGES_DIR} PACKAGES_DIR)

option(rdepSync
    "Whether rdep should sync packages or use only existing copies"
    ON)

function(nx_rdep_configure)
    set(ENV{RDEP_PACKAGES_DIR} ${PACKAGES_DIR})
    execute_process(COMMAND ${PYTHON_EXECUTABLE} ${rdep_dir}/rdep_configure.py
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Cannot configure RDep repository.")
    endif()

    if(rdepSync)
        execute_process(COMMAND
            ${PYTHON_EXECUTABLE} ${rdep_dir}/rdep.py --sync-timestamps --root ${PACKAGES_DIR})
    endif()
endfunction()

function(_rdep_get_full_package_name package var)
    string(FIND ${package} "/" pos)
    if(pos EQUAL -1)
        set(target ${rdep_target})
    else()
        string(SUBSTRING ${package} 0 ${pos} target)
        math(EXPR pos "${pos} + 1")
        string(SUBSTRING ${package} ${pos} -1 package)
    endif()

    set(version ${${package}_version})
    if(version)
        set(package ${package}-${version})
    endif()

    set(${var} "${target}/${package}" PARENT_SCOPE)
endfunction()

function(nx_rdep_sync_package package)
    cmake_parse_arguments(RDEP "" "RESULT_VARIABLE" "" ${ARGN})

    message(STATUS "Syncing package ${package}")
    execute_process(
        COMMAND
            ${PYTHON_EXECUTABLE}
            ${rdep_dir}/sync.py
            ${package}
        RESULT_VARIABLE result)

    if(RDEP_RESULT_VARIABLE)
        set(${RDEP_RESULT_VARIABLE} ${result} PARENT_SCOPE)
    endif()
endfunction()

function(_rdep_load_package_from_dir package_dir)
    file(GLOB cmake_files "${package_dir}/*.cmake")
    foreach(file ${cmake_files})
        message(STATUS "Loading file ${file}")
        include("${file}")
    endforeach()
endfunction()

function(_rdep_try_package package)
    cmake_parse_arguments(RDEP "" "PATH_VARIABLE;RESULT_VARIABLE" "" ${ARGN})

    set(package_dir "${PACKAGES_DIR}/${package}")

    if(rdepSync)
        nx_rdep_sync_package(${package} RESULT_VARIABLE result)
    else()
        set(result 0)
        message(STATUS "Looking for the existing package ${package}")
    endif()

    if(IS_DIRECTORY "${package_dir}")
        if(NOT result EQUAL 0)
            message(WARNING "Could not sync package ${package}. Using the existing copy in ${package_dir}.")
        endif()

        _rdep_load_package_from_dir("${package_dir}")

        if(NOT EXISTS "${package_dir}/.nocopy")
            nx_copy_package(${package_dir})
        endif()

        if(RDEP_PATH_VARIABLE)
            set(${RDEP_PATH_VARIABLE} "${package_dir}" PARENT_SCOPE)
        endif()
        if(RDEP_RESULT_VARIABLE)
            set(${RDEP_RESULT_VARIABLE} 1 PARENT_SCOPE)
        endif()
    else()
        if(RDEP_RESULT_VARIABLE)
            set(${RDEP_RESULT_VARIABLE} 0 PARENT_SCOPE)
        endif()
    endif()
endfunction()

function(nx_rdep_add_package package)
    cmake_parse_arguments(RDEP "OPTIONAL" "PATH_VARIABLE" "" ${ARGN})

    _rdep_get_full_package_name(${package} package)

    set(package_found 0)

    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        _rdep_try_package("${package}-debug"
            PATH_VARIABLE package_dir
            RESULT_VARIABLE package_found)
    endif()

    if(NOT package_found)
        _rdep_try_package("${package}"
            PATH_VARIABLE package_dir
            RESULT_VARIABLE package_found)
    endif()

    if(NOT package_found)
        if(RDEP_OPTIONAL)
            message(STATUS "Package ${package} not found")
        else()
            message(FATAL_ERROR "Package ${package} not found")
        endif()
        return()
    endif()

    if(RDEP_PATH_VARIABLE)
        set(${RDEP_PATH_VARIABLE} "${package_dir}" PARENT_SCOPE)
    endif()
endfunction()
