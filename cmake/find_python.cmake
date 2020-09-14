function(verify_python_at_least_37)
    execute_process(
        COMMAND "${PYTHON_EXECUTABLE}" --version
        OUTPUT_VARIABLE PYTHON_VERSION
        ERROR_VARIABLE PYTHON_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if (NOT "${PYTHON_VERSION}" MATCHES "Python 3\.(7|8)(\..*)?")
        set(PYTHON_EXECUTABLE PYTHON_EXECUTABLE-NOTFOUND PARENT_SCOPE)
        return()
    endif()

    message(STATUS "Found python executable: '${PYTHON_EXECUTABLE}'.")
endfunction()

function(find_python_at_least_37_in_dir)
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

        if(NOT ${PYTHON_EXECUTABLE} STREQUAL PYTHON_EXECUTABLE-NOTFOUND)
            verify_python_at_least_37()
        endif()

        if(NOT ${PYTHON_EXECUTABLE} STREQUAL PYTHON_EXECUTABLE-NOTFOUND)
            return()
        endif()
    endforeach ()
endfunction()

function(find_python_at_least_37)
    set(SEARCH_PATHS_LIST $ENV{PATH})

    if(CYGWIN OR NOT WIN32)
        string(REPLACE ":" ";" SEARCH_PATHS_LIST ${SEARCH_PATHS_LIST})
    endif()

    foreach(SEARCH_PATH IN LISTS SEARCH_PATHS_LIST)
        message("Searching Python in \"${SEARCH_PATH}\"...")

        find_python_at_least_37_in_dir(PATTERNS ${ARGV} SEARCH_PATH ${SEARCH_PATH})

        if(NOT ${PYTHON_EXECUTABLE} STREQUAL PYTHON_EXECUTABLE-NOTFOUND)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "Python >=3.7 executable not found.")
endfunction()

if("${PYTHON_EXECUTABLE}" STREQUAL PYTHON_EXECUTABLE-NOTFOUND
    OR "${PYTHON_EXECUTABLE}" STREQUAL ""
)
    find_python_at_least_37(python3.8 python3.7 python3 python)
else()
    message(STATUS "Found python executable: '${PYTHON_EXECUTABLE}'.")
endif()

set(ENV{PYTHONDONTWRITEBYTECODE} True)

