find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/nx_utils.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")

nx_configure_file(
    maven/filter-resources/app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

add_library(nx_utils
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/nx_utils.qrc)

add_precompiled_header(nx_utils src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_utils PUBLIC src)

target_compile_definitions(nx_utils
    PRIVATE NX_UTILS_API=${API_EXPORT_MACRO}
    INTERFACE NX_UTILS_API=${API_IMPORT_MACRO})

target_link_libraries(nx_utils PUBLIC boost Qt5::Core)
