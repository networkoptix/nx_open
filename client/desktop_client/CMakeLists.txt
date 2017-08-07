nx_add_target(desktop_client EXECUTABLE
    PUBLIC_LIBS qtsingleapplication nx_kit nx_client_desktop)

if(WINDOWS AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(
        TARGET desktop_client
        COMMAND ${CMAKE_COMMAND}
        ARGS -E copy "${bin_source_dir}/desktop_client.exe" "${bin_source_dir}/${client.binary.name}"
    )
endif()