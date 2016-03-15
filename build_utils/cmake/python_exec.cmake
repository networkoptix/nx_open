if(WIN32)
    #TODO Fix for x86 builders
    set(PYTHON_EXECUTABLE $ENV{environment}/python/x64/python.exe)
else()
    set(PYTHON_EXECUTABLE python)
endif()

set(PYTHON_MODULES_DIR ${CMAKE_SOURCE_DIR}/build_utils/python)
