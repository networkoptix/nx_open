set(RDEP_DIR "${PROJECT_SOURCE_DIR}/build_utils/python" CACHE PATH "Path to rdep scripts")
mark_as_advanced(RDEP_DIR)
set(PACKAGES_DIR "$ENV{environment}/packages" CACHE STRING "Path to local rdep repository")
mark_as_advanced(PACKAGES_DIR)

function(rdep_configure)
    execute_process(COMMAND ${PYTHON_EXECUTABLE} ${RDEP_DIR}/rdep_configure.py)
endfunction()

function(get_full_package_name package var)
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

function(rdep_sync_package package)
    execute_process(
        COMMAND
            ${PYTHON_EXECUTABLE}
            ${RDEP_DIR}/sync.py
            ${package}
        RESULT_VARIABLE RDEP_RESULT)
endfunction()

function(rdep_add_package package)
    cmake_parse_arguments(RDEP "OPTIONAL" "PATH_VARIABLE" "" ${ARGN})

    get_full_package_name(${package} package)

    set(package_dir ${PACKAGES_DIR}/${package})

    if(rdepSync)
        rdep_sync_package(${package})
    else()
        message(STATUS "Using existing package ${package}")
    endif()

    if(NOT IS_DIRECTORY ${package_dir})
        if(RDEP_OPTIONAL)
            message(STATUS "Package ${package} not found")
            return()
        else()
            message(FATAL_ERROR "Package ${package} not found")
        endif()
    endif()

    file(GLOB CMAKE_FILES ${package_dir}/*.cmake)
    foreach(file ${CMAKE_FILES})
        include(${file})
    endforeach()

    if(RDEP_PATH_VARIABLE)
        set(${RDEP_PATH_VARIABLE} ${package_dir} PARENT_SCOPE)
    endif()
endfunction()
