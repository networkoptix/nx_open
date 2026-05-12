## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Invoked via `cmake -P` from nx_add_functional_test.
# Walks BINARY's runtime deps, drops system libs, copies the rest (with symlink chains) into DEST.
#
# Required:
#   BINARY  - absolute path to the ELF to inspect
#   DEST    - absolute path to the directory to populate (created if missing)
# Optional:
#   EXTRA_PRE_EXCLUDE   - extra name regexes (matched before path resolution)
#   EXTRA_POST_EXCLUDE  - extra path regexes (matched after path resolution)

if(NOT BINARY)
    message(FATAL_ERROR "copy_runtime_deps: BINARY is not set")
endif()
if(NOT DEST)
    message(FATAL_ERROR "copy_runtime_deps: DEST is not set")
endif()
if(NOT EXISTS "${BINARY}")
    message(FATAL_ERROR "copy_runtime_deps: BINARY does not exist: ${BINARY}")
endif()

set(pre_exclude "")
if(EXTRA_PRE_EXCLUDE)
    list(APPEND pre_exclude ${EXTRA_PRE_EXCLUDE})
endif()

# Drop anything that resolved to a system path. Build-tree and Conan-cache libs are kept.
set(post_exclude
    "^/lib(32|64|x32)?/"
    "^/usr/lib(32|64|x32)?/"
    "^/usr/local/lib(32|64|x32)?/"
)
if(EXTRA_POST_EXCLUDE)
    list(APPEND post_exclude ${EXTRA_POST_EXCLUDE})
endif()

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${BINARY}"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    PRE_EXCLUDE_REGEXES ${pre_exclude}
    POST_EXCLUDE_REGEXES ${post_exclude}
)

file(MAKE_DIRECTORY "${DEST}")

foreach(lib IN LISTS resolved)
    file(COPY "${lib}" DESTINATION "${DEST}" FOLLOW_SYMLINK_CHAIN)
endforeach()

if(unresolved)
    message(STATUS "copy_runtime_deps: unresolved dependencies for ${BINARY}:")
    foreach(u IN LISTS unresolved)
        message(STATUS "  ${u}")
    endforeach()
endif()
