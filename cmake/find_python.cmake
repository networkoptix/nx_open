if(WIN32)
    find_program(PYTHON_EXECUTABLE python.exe)
    if(PYTHON_EXECUTABLE)
        message(STATUS "Found python executable: ${PYTHON_EXECUTABLE}")
    endif()
else()
    find_package(PythonInterp 2 REQUIRED)
endif()

if(NOT PYTHON_EXECUTABLE)
    message(FATAL_ERROR "Python executable not found.")
endif()

set(ENV{PYTHONDONTWRITEBYTECODE} True)
