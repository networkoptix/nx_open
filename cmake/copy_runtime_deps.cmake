## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

# Walks BINARY's runtime deps, drops system libs, copies the rest (with symlink chains) into DEST.
#
# SYSTEM_ROOTS - roots providing the target system libs (cross-build sysroot, os_deps package).
# Searched for dependencies, then excluded like the host system paths.
function(nx_copy_runtime_deps binary dest)
    cmake_parse_arguments(ARG "" "" "SYSTEM_ROOTS" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "nx_copy_runtime_deps: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT EXISTS "${binary}")
        message(FATAL_ERROR "nx_copy_runtime_deps: binary does not exist: ${binary}")
    endif()

    set(system_root_dirs "")
    set(system_root_excludes "")
    foreach(root IN LISTS ARG_SYSTEM_ROOTS)
        file(GLOB root_dirs
            "${root}/lib"
            "${root}/lib64"
            "${root}/lib/*-linux-gnu*"
            "${root}/usr/lib"
            "${root}/usr/lib64"
            "${root}/usr/lib/*-linux-gnu*")
        list(APPEND system_root_dirs ${root_dirs})

        string(REGEX REPLACE "([.+])" "\\\\\\1" root_regex "${root}")
        list(APPEND system_root_excludes "^${root_regex}/")
    endforeach()

    # Drop anything that resolved to a system path. Build-tree and Conan-cache libs are kept.
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${binary}"
        RESOLVED_DEPENDENCIES_VAR resolved
        UNRESOLVED_DEPENDENCIES_VAR unresolved
        DIRECTORIES ${system_root_dirs}
        POST_EXCLUDE_REGEXES
            "^/lib(32|64|x32)?/"
            "^/usr/lib(32|64|x32)?/"
            "^/usr/local/lib(32|64|x32)?/"
            ${system_root_excludes}
    )

    if(unresolved)
        string(REPLACE ";" "\n  " unresolved_text "${unresolved}")
        message(FATAL_ERROR
            "nx_copy_runtime_deps: unresolved dependencies of ${binary}:\n  ${unresolved_text}")
    endif()

    file(MAKE_DIRECTORY "${dest}")
    foreach(lib IN LISTS resolved)
        file(COPY "${lib}" DESTINATION "${dest}" FOLLOW_SYMLINK_CHAIN)
    endforeach()
endfunction()
