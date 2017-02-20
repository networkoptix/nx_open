nx_add_target(nx_fusion_ut EXECUTABLE PUBLIC_LIBS nx_fusion GTest GMock)
target_include_directories(nx_fusion_ut PRIVATE ${CMAKE_SOURCE_DIR}/common/src)
