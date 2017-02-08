find_sources(src CPP_FILES HPP_FILES)

add_library(genericrtspplugin ${CPP_FILES})

target_link_libraries(genericrtspplugin nx_streaming)
