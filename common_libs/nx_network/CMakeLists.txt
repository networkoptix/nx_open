set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

if(NOT WINDOWS)
    add_compile_options("-Werror" "-Wall" "-Wextra")
endif()

nx_configure_file(
    maven/filter-resources/app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(nx_network LIBRARY ${library_type}
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp"
    PUBLIC_LIBS
        Qt5::Network Qt5::Concurrent
        openssl
        nx_kit
        nx_utils nx_fusion udt
)

target_compile_definitions(nx_network
    PRIVATE NX_NETWORK_API=${API_EXPORT_MACRO}
    INTERFACE NX_NETWORK_API=${API_IMPORT_MACRO})

if(WINDOWS)
    target_link_libraries(nx_network PRIVATE Iphlpapi ws2_32)
endif()
