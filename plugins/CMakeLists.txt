set(BUILD_SHARED_LIBS ON)

if(CMAKE_MULTI_CONFIGURATION_MODE)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${config} CONFIG)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG}
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG}}/plugins")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG}
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG}}/plugins")
    endforeach()
else()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins)
endif()

add_subdirectory(genericrtspplugin)
add_subdirectory(image_library_plugin)
add_subdirectory(mjpg_link)

if(LINUX AND box STREQUAL "none")
    add_subdirectory(it930x_plugin)
endif()

if(box MATCHES "isd")
    add_subdirectory(isd_native_plugin)
endif()
