find_sources(src CPP_FILES HPP_FILES)

add_library(it930x_plugin ${CPP_FILES})

target_include_directories(it930x_plugin PRIVATE src)
target_compile_definitions(it930x_plugin PRIVATE DTVAPI_V15)
target_link_libraries(it930x_plugin nx_streaming)
