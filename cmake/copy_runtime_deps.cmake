## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

# Dependencies the FT base images install from the distro instead of staging from the build.
set(nx_allowed_unresolved_runtime_deps "libOpenGL.so.0")

# Walks BINARY's runtime deps, drops system libs, copies the rest (with symlink chains) into DEST.
#
# ALLOWED_UNRESOLVED - names allowed to stay unresolved; anything else is an error. Defaults to
# nx_allowed_unresolved_runtime_deps.
function(nx_copy_runtime_deps binary dest)
    cmake_parse_arguments(ARG "" "" "ALLOWED_UNRESOLVED" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "nx_copy_runtime_deps: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT EXISTS "${binary}")
        message(FATAL_ERROR "nx_copy_runtime_deps: binary does not exist: ${binary}")
    endif()

    if(NOT DEFINED ARG_ALLOWED_UNRESOLVED)
        set(ARG_ALLOWED_UNRESOLVED ${nx_allowed_unresolved_runtime_deps})
    endif()

    # Drop anything that resolved to a system path. Build-tree and Conan-cache libs are kept.
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${binary}"
        RESOLVED_DEPENDENCIES_VAR resolved
        UNRESOLVED_DEPENDENCIES_VAR unresolved
        POST_EXCLUDE_REGEXES
            "^/lib(32|64|x32)?/"
            "^/usr/lib(32|64|x32)?/"
            "^/usr/local/lib(32|64|x32)?/"
    )

    if(unresolved AND ARG_ALLOWED_UNRESOLVED)
        list(REMOVE_ITEM unresolved ${ARG_ALLOWED_UNRESOLVED})
    endif()
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
