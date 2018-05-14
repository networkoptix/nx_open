find_package(java COMPONENTS Runtime)

if(NOT Java_JAVA_EXECUTABLE)
    message(FATAL_ERROR "Java Runtime not found.")
endif()
