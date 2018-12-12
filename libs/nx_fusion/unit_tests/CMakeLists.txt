nx_add_test(nx_fusion_ut
    PUBLIC_LIBS
        nx_fusion)
target_include_directories(nx_fusion_ut PRIVATE ${CMAKE_SOURCE_DIR}/vms/libs/common/src)
