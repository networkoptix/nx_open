rdep_add_package(openssl)
rdep_add_package(ffmpeg)
rdep_add_package(quazip)

nx_configure_file("${CMAKE_CURRENT_SOURCE_DIR}/qt.conf"
    ${CMAKE_CURRENT_BINARY_DIR}/resources/qt/etc/qt.conf)

nx_configure_file(
    maven/filter-resources/app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(common LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp"
    ADDITIONAL_RESOURCES
        "${customization_dir}/common/resources"
        "${CMAKE_CURRENT_BINARY_DIR}/resources"
    PUBLIC_LIBS
        Qt5::Gui Qt5::Concurrent Qt5::Network Qt5::Multimedia
        Qt5::Xml Qt5::XmlPatterns Qt5::Sql
        openssl ffmpeg quazip
        nx_utils nx_fusion)

target_include_directories(common
    PRIVATE
        ${Qt5Core_PRIVATE_INCLUDE_DIRS}
        "${CMAKE_SOURCE_DIR}/common_libs/nx_streaming/src"
    PUBLIC
        "${CMAKE_SOURCE_DIR}/common_libs/nx_network/src")

target_compile_definitions(common
    PUBLIC NX_NETWORK_API=${API_IMPORT_MACRO})
