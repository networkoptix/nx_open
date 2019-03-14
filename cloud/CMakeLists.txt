add_subdirectory(backend)

if(withClouds AND LINUX)
    add_subdirectory(deploy)
endif()
