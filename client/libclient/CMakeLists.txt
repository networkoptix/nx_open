set(product.title "${client.name}")
set(product.display.title "${client.display.name}")

find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

find_files(src "ui" UI_FILES)
qt5_wrap_ui(UIC_FILES ${UI_FILES})

nx_configure_file(
    maven/filter-resources/resources/globals.json
    ${CMAKE_CURRENT_BINARY_DIR})

generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/libclient.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources"
    "${customization_dir}/libclient/resources"
    "${CMAKE_CURRENT_BINARY_DIR}/globals.json")

nx_configure_file(
    maven/filter-resources/client_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    maven/filter-resources/version.h
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    ${PROJECT_SOURCE_DIR}/cpp/maven/bin-resources/app_icons.h
    ${CMAKE_CURRENT_BINARY_DIR})

file(GLOB_RECURSE QML_FILES static-resources/src/qml/*.qml static-resources/src/qml/*.js)

add_library(client
    ${CPP_FILES} ${MOC_FILES} ${UIC_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/client_app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/libclient.qrc
    ${QML_FILES})

add_precompiled_header(client src/StdAfx.h FORCEINCLUDE)

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
        client_core nx_speech_synthesizer nx_audio libvms_gateway)

if(LINUX)
    target_link_libraries(client PRIVATE Qt5::X11Extras Xss Xfixes X11)
endif()
