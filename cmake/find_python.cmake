set(Python3_FIND_STRATEGY LOCATION)
set(Python3_FIND_FRAMEWORK NEVER)
find_package(Python3 COMPONENTS Interpreter)

if(Python3_FOUND)
    set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
    message(STATUS "Found Python 3 executable: ${PYTHON_EXECUTABLE}")
else()
    message(FATAL_ERROR "Python 3 executable not found.")
endif()

set(ENV{PYTHONDONTWRITEBYTECODE} True)
