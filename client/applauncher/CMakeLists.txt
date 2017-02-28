set(product.title "${applauncher.name}")

nx_configure_file(maven/filter-resources/applauncher_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(applauncher EXECUTABLE
    ADDITIONAL_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/applauncher_app_info_impl.cpp"
    PUBLIC_LIBS qtsinglecoreapplication nx_network)

target_link_libraries(applauncher
    PUBLIC
        -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        -lcommon -lnx_streaming -lnx_network)
