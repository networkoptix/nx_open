nx_add_test(nx_fusion_ut PUBLIC_LIBS nx_fusion GTest GMock)
target_include_directories(nx_fusion_ut PRIVATE ${CMAKE_SOURCE_DIR}/common/src)
