find_sources(src CPP_FILES)
generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/common.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources"
    "${customization_dir}/common/resources"
)

configure_file(
    maven/filter-resources/app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp
)

add_library(common
    ${CPP_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/common.qrc
)

add_precompiled_header(common src/StdAfx.h FORCEINCLUDE)

rdep_add_package(openssl)
rdep_add_package(ffmpeg)
rdep_add_package(quazip)

target_include_directories(common
    PRIVATE
        ${Qt5Core_PRIVATE_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/common_libs/nx_streaming/src
    PUBLIC
        ${CMAKE_SOURCE_DIR}/common_libs/nx_network/src
        src
)

target_compile_definitions(common
    PUBLIC NX_NETWORK_API=${API_IMPORT_MACRO})

target_link_libraries(common
    PUBLIC
        Qt5::Gui Qt5::Concurrent Qt5::Network Qt5::Multimedia
        Qt5::Xml Qt5::XmlPatterns Qt5::Sql
        openssl ffmpeg quazip
        nx_utils nx_fusion)
