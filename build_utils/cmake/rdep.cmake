set(RDEP_DIR $ENV{HOME}/develop/devtools/rdep CACHE PATH "Path to rdep scripts")
set(PACKAGES_DIR $ENV{environment}/packages CACHE STRING "Path to local rdep repository")

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
    get_full_package_name(${package} package)
    if(rdepSync)
        rdep_sync_package(${package})
    endif()

    set(package_dir ${PACKAGES_DIR}/${package})
    include(${package_dir}/package.cmake OPTIONAL)
endfunction()
