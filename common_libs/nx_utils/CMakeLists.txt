find_sources(src CPP_FILES)
generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/nx_utils.qrc "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")

add_library(nx_utils
    ${CPP_FILES}
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/nx_utils.qrc)

add_precompiled_header(nx_utils src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_utils PUBLIC src)

target_compile_definitions(nx_utils
    PRIVATE NX_UTILS_API=${API_EXPORT_MACRO}
    INTERFACE NX_UTILS_API=${API_IMPORT_MACRO})

target_link_libraries(nx_utils PUBLIC boost Qt5::Core)
