find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/mobile_client.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources"
    "${customization_dir}/mobile_client/resources")

nx_configure_file(
    maven/filter-resources/mobile_client_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

add_executable(mobile_client
    ${CPP_FILES} ${MOC_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/mobile_client_app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/mobile_client.qrc)

target_include_directories(mobile_client PRIVATE src)

add_precompiled_header(mobile_client src/StdAfx.h FORCEINCLUDE)

find_package(Qt5 COMPONENTS WebChannel WebSockets)

target_link_libraries(mobile_client
    Qt5::WebChannel Qt5::WebSockets
    qtsingleguiapplication
    client_core)
