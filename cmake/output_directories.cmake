set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

foreach(_type LIBRARY RUNTIME)
    file(MAKE_DIRECTORY "${CMAKE_${_type}_OUTPUT_DIRECTORY}")

    if(CMAKE_MULTI_CONFIGURATION_MODE)
        foreach(_config ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${_config} _CONFIG)
            set(CMAKE_${_type}_OUTPUT_DIRECTORY_${_CONFIG}
                "${CMAKE_${_type}_OUTPUT_DIRECTORY}/${_config}")
            file(MAKE_DIRECTORY "${CMAKE_${_type}_OUTPUT_DIRECTORY}/${_config}")
        endforeach()
    endif()
endforeach()

unset(_type)
unset(_config)
unset(_CONFIG)
