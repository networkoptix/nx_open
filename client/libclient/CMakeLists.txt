set(CMAKE_AUTOUIC ON)

find_sources(src CPP_FILES)

configure_file(
    maven/filter-resources/resources/globals.json
    ${CMAKE_CURRENT_BINARY_DIR})
generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/client.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources"
    "${customization_dir}/client/resources"
    "${CMAKE_CURRENT_BINARY_DIR}/globals.json"
)

configure_file(
    maven/filter-resources/client_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})
configure_file(
    maven/filter-resources/version.h
    ${CMAKE_CURRENT_BINARY_DIR})
configure_file(
    ${PROJECT_SOURCE_DIR}/cpp/maven/bin-resources/app_icons.h
    ${CMAKE_CURRENT_BINARY_DIR})

add_library(client
    ${CPP_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/client_app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/client.qrc
)

add_precompiled_header(client src/StdAfx.h FORCEINCLUDE)

rdep_add_package(any/festival-vox)
rdep_add_package(any/roboto-ttf)
rdep_add_package(any/help)

target_include_directories(client
    PRIVATE
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${Qt5Widgets_PRIVATE_INCLUDE_DIRS}
    PUBLIC src
)

target_link_libraries(client
    PUBLIC
        Qt5::Widgets Qt5::OpenGL Qt5::WebKit Qt5::WebKitWidgets
        Qt5::Qml Qt5::QuickWidgets
        client_core nx_speech_synthesizer nx_audio vms_gateway
)
if(LINUX)
    target_link_libraries(client PRIVATE Qt5::X11Extras)
endif()
