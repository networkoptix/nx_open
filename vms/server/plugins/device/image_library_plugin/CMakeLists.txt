nx_add_server_plugin(image_library_plugin NO_MOC NO_GTEST OPTIONAL
    PRIVATE_LIBS
        nx_sdk
        nx_kit
)

target_include_directories(image_library_plugin PRIVATE "${PROJECT_SOURCE_DIR}/common/src")
