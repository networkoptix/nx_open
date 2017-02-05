find_sources(src CPP_FILES HPP_FILES)

add_library(image_library_plugin ${CPP_FILES})

target_include_directories(image_library_plugin
    PRIVATE "${PROJECT_SOURCE_DIR}/common/src")

target_link_libraries(image_library_plugin Qt5::Core)
