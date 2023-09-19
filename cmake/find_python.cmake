## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

function(verify_python_version_is_compatible)
    execute_process(
        COMMAND "${PYTHON_EXECUTABLE}" --version
        OUTPUT_VARIABLE PYTHON_VERSION
        ERROR_VARIABLE PYTHON_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if (NOT "${PYTHON_VERSION}" MATCHES "Python 3\.(8|9|10|11)(\..*)?")
        set(PYTHON_EXECUTABLE PYTHON_EXECUTABLE-NOTFOUND PARENT_SCOPE)
    endif()
endfunction()

function(find_compatible_python_version_in_dir)
    cmake_parse_arguments("" "" "SEARCH_PATH" "PATTERNS" "${ARGN}")

    foreach(PATTERN IN LISTS _PATTERNS)
        if(CYGWIN)
            set(
                PYTHON_EXECUTABLE_PATTERNS
                ${PATTERN}
                ${PATTERN}.exe
            )
        elseif(WIN32)
            set(PYTHON_EXECUTABLE_PATTERNS ${PATTERN}.exe)
        else()
            set(PYTHON_EXECUTABLE_PATTERNS ${PATTERN})
        endif()

        find_program(
            PYTHON_EXECUTABLE NAMES ${PYTHON_EXECUTABLE_PATTERNS}
            PATHS ${_SEARCH_PATH}
            NO_DEFAULT_PATH
        )

        if(PYTHON_EXECUTABLE)
            verify_python_version_is_compatible()
        endif()

        set(PYTHON_EXECUTABLE ${PYTHON_EXECUTABLE} PARENT_SCOPE)

        if(PYTHON_EXECUTABLE)
            return()
        endif()
    endforeach ()
endfunction()

function(find_compatible_python_version)
    set(SEARCH_PATHS_LIST $ENV{PATH})

    if(CYGWIN OR NOT WIN32)
        string(REPLACE ":" ";" SEARCH_PATHS_LIST ${SEARCH_PATHS_LIST})
    endif()

    foreach(SEARCH_PATH IN LISTS SEARCH_PATHS_LIST)
        message(DEBUG "Searching Python in \"${SEARCH_PATH}\"...")

        find_compatible_python_version_in_dir(PATTERNS ${ARGV} SEARCH_PATH ${SEARCH_PATH})

        if(PYTHON_EXECUTABLE)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "Python 3.8/9/10/11 executable not found.")
endfunction()

if(NOT PYTHON_EXECUTABLE)
    find_compatible_python_version(python3.11 python3.10 python3.9 python3.8 python3 python)
endif()

message(STATUS "Found python executable: '${PYTHON_EXECUTABLE}'.")

set(ENV{PYTHONDONTWRITEBYTECODE} True)
