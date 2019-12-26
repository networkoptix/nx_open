if(WIN32)
    find_program(PYTHON_EXECUTABLE python.exe)
else()
    find_program(PYTHON_EXECUTABLE python3)
endif()

if(PYTHON_EXECUTABLE)
    message(STATUS "Found python executable: ${PYTHON_EXECUTABLE}")
else()
    message(FATAL_ERROR "Python executable not found.")
endif()

set(ENV{PYTHONDONTWRITEBYTECODE} True)
