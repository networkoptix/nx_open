nx_add_target(image_library_plugin LIBRARY
    NO_MOC NO_PCH PUBLIC_LIBS Qt5::Core)

target_include_directories(image_library_plugin
    PRIVATE "${PROJECT_SOURCE_DIR}/common/src")
