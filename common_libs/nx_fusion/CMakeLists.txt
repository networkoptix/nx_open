nx_add_target(nx_fusion LIBRARY PUBLIC_LIBS Qt5::Gui nx_utils)

target_include_directories(nx_fusion PRIVATE ${CMAKE_SOURCE_DIR}/common/src)

target_compile_definitions(nx_fusion
    PRIVATE NX_FUSION_API=${API_EXPORT_MACRO}
    INTERFACE NX_FUSION_API=${API_IMPORT_MACRO})
