## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Invoked via `cmake -P` from nx_add_functional_test on every build: brings DEST_DIR back in sync
# with the build tree and touches STAMP_FILE only when something changed. Running unconditionally
# is what makes the staging self-healing - nothing else in the build graph tracks the contents of
# DEST_DIR, so a directory that lost files between builds would otherwise be packaged as is.
#
# Required:
#   DEST_DIR        - directory to populate
#   STAMP_FILE      - file touched when DEST_DIR changed
# Optional:
#   FILES              - files to copy into DEST_DIR
#   BINARIES           - executables to copy into DEST_DIR, runtime deps into DEST_DIR/lib
#   QT_PLUGIN_DIRS     - plugin group directories to copy into DEST_DIR/plugins
#   EXTRA_LIBS         - libraries to copy into DEST_DIR/lib
#   ALLOWED_UNRESOLVED - runtime deps of BINARIES allowed to stay unresolved

include("${CMAKE_CURRENT_LIST_DIR}/copy_runtime_deps.cmake")

if(NOT DEST_DIR)
    message(FATAL_ERROR "stage_functional_test: DEST_DIR is not set")
endif()
if(NOT STAMP_FILE)
    message(FATAL_ERROR "stage_functional_test: STAMP_FILE is not set")
endif()

function(directory_signature dir result)
    file(GLOB_RECURSE entries LIST_DIRECTORIES false "${dir}/*")
    list(SORT entries)

    set(signature "")
    foreach(entry IN LISTS entries)
        file(SIZE "${entry}" size)
        file(TIMESTAMP "${entry}" timestamp UTC)
        string(APPEND signature "${entry} ${size} ${timestamp}\n")
    endforeach()

    set(${result} "${signature}" PARENT_SCOPE)
endfunction()

function(copy_into source destination_dir)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "stage_functional_test: ${source} does not exist")
    endif()
    file(COPY "${source}" DESTINATION "${destination_dir}" ${ARGN})
endfunction()

file(MAKE_DIRECTORY "${DEST_DIR}")
directory_signature("${DEST_DIR}" signature_before)

foreach(file IN LISTS FILES)
    copy_into("${file}" "${DEST_DIR}")
endforeach()

# Passing the keyword with an empty value would drop the default allowlist, not extend it.
set(allowed_unresolved "")
if(ALLOWED_UNRESOLVED)
    set(allowed_unresolved ALLOWED_UNRESOLVED ${ALLOWED_UNRESOLVED})
endif()

foreach(binary IN LISTS BINARIES)
    copy_into("${binary}" "${DEST_DIR}")
    nx_copy_runtime_deps("${binary}" "${DEST_DIR}/lib" ${allowed_unresolved})
endforeach()

foreach(plugin_dir IN LISTS QT_PLUGIN_DIRS)
    copy_into("${plugin_dir}" "${DEST_DIR}/plugins")
endforeach()

foreach(lib IN LISTS EXTRA_LIBS)
    copy_into("${lib}" "${DEST_DIR}/lib" FOLLOW_SYMLINK_CHAIN)
endforeach()

directory_signature("${DEST_DIR}" signature_after)

if(NOT EXISTS "${STAMP_FILE}" OR NOT signature_before STREQUAL signature_after)
    file(TOUCH "${STAMP_FILE}")
endif()
