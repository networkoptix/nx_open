nx_add_target(nx_streaming LIBRARY PUBLIC_LIBS common)

target_include_directories(nx_streaming PRIVATE "${CMAKE_SOURCE_DIR}/common_libs/nx_network/src")

target_compile_definitions(nx_streaming PRIVATE NX_NETWORK_API=${API_IMPORT_MACRO})
