function(check_python_at_least_37)
    execute_process(
        COMMAND "${PYTHON_EXECUTABLE}" --version
        OUTPUT_VARIABLE PYTHON_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT "${PYTHON_VERSION}" MATCHES "Python 3\.(7|8)(\..*)?")
        if ("${PYTHON_VERSION}" MATCHES "Python (.*)")
              message(
                  FATAL_ERROR
                  "Executable ${PYTHON_EXECUTABLE} has invalid version: "
                  "${CMAKE_MATCH_1} instead of 3.(7|8)*."
              )
         else()
              message(FATAL_ERROR "Unexpected Python version: '${PYTHON_VERSION}'.")
         endif()
    endif()
endfunction()

function(find_python_at_least_37)
    if(${ARGC} GREATER 1)
        list(SUBLIST ARGV 1 ${ARGC} ARGV_EXCEPT_FIRST)
    endif()

    set(PYTHON_EXECUTABLE_PATTERNS ${ARGV0})

    if (CYGWIN)
        set(PYTHON_EXECUTABLE_PATTERNS ${PYTHON_EXECUTABLE_PATTERNS} ${PYTHON_EXECUTABLE_PATTERNS}.exe)
    elseif(WIN32)
        set(PYTHON_EXECUTABLE_PATTERNS ${PYTHON_EXECUTABLE_PATTERNS}.exe)
    endif()

    find_program(
        PYTHON_EXECUTABLE NAMES ${PYTHON_EXECUTABLE_PATTERNS} NO_CMAKE_SYSTEM_PATH NO_CMAKE_PATH
    )

    if (${PYTHON_EXECUTABLE} STREQUAL PYTHON_EXECUTABLE-NOTFOUND)
        if(${ARGC} EQUAL 1)
            message(FATAL_ERROR "Python >=3.7 executable not found.")
        endif()

        find_python_at_least_37(${ARGV_EXCEPT_FIRST})
    else()
        message(STATUS "Found python executable: '${PYTHON_EXECUTABLE}'.")

        check_python_at_least_37()

        return()
    endif()
endfunction()

find_python_at_least_37(python3.8 python3.7 python3 python)

set(ENV{PYTHONDONTWRITEBYTECODE} True)

