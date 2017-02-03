find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

add_library(nx_fusion
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h)

add_precompiled_header(nx_fusion src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_fusion
    PRIVATE ${CMAKE_SOURCE_DIR}/common/src
    PUBLIC src)

target_compile_definitions(nx_fusion
    PRIVATE NX_FUSION_API=${API_EXPORT_MACRO}
    INTERFACE NX_FUSION_API=${API_IMPORT_MACRO})

target_link_libraries(nx_fusion PUBLIC Qt5::Gui nx_utils)
