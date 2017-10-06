if(LINUX)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_configure_file(
    "maven/filter-resources/time_server_app_info_impl.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}")

nx_add_target(time_server EXECUTABLE
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/time_server_app_info_impl.cpp"
    PUBLIC_LIBS
        nx_network nx_utils
)
