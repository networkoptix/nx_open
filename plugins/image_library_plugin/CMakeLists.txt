nx_add_server_plugin(image_library_plugin NO_MOC OPTIONAL
    PRIVATE_LIBS
        Qt5::Core
        nx_plugin_utils
)

target_include_directories(image_library_plugin PRIVATE "${PROJECT_SOURCE_DIR}/common/src")
