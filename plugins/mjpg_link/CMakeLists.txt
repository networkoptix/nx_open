find_sources(src CPP_FILES HPP_FILES)

add_library(mjpg_link ${CPP_FILES})

target_link_libraries(mjpg_link nx_network)
