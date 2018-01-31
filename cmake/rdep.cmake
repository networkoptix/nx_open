set(RDEP_DIR "${CMAKE_SOURCE_DIR}/build_utils/python" CACHE PATH "Path to rdep scripts")
mark_as_advanced(RDEP_DIR)
set(PACKAGES_DIR "$ENV{environment}/packages" CACHE STRING "Path to local rdep repository")
mark_as_advanced(PACKAGES_DIR)

option(rdepSync
    "Whether rdep should sync packages or use only existing copies"
    ON)

function(nx_rdep_configure)
    execute_process(COMMAND ${PYTHON_EXECUTABLE} ${RDEP_DIR}/rdep_configure.py)
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
            ${RDEP_DIR}/sync.py
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
    cmake_parse_arguments(RDEP "DEBUG_PACKAGE_FOUND" "PATH_VARIABLE;RESULT_VARIABLE" "" ${ARGN})

    set(package_dir "${PACKAGES_DIR}/${package}")

    if(package MATCHES "-debug$")
        set(debug_package TRUE)
    endif()

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
            if(CMAKE_MULTI_CONFIGURATION_MODE)
                if(debug_package)
                    set(configurations "Debug")
                else()
                    set(configurations ${CMAKE_ACTIVE_CONFIGURATIONS})
                    if(RDEP_DEBUG_PACKAGE_FOUND)
                        list(REMOVE_ITEM configurations "Debug")
                    endif()
                endif()
            else()
                set(configurations)
            endif()

            nx_copy_package("${package_dir}" CONFIGURATIONS ${configurations})
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

        if(package_found)
            set(debug_package_found DEBUG_PACKAGE_FOUND)
        endif()
    endif()

    if(NOT package_found OR CMAKE_MULTI_CONFIGURATION_MODE)
        _rdep_try_package("${package}"
             PATH_VARIABLE package_dir
             RESULT_VARIABLE package_found
             ${debug_package_found})
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
