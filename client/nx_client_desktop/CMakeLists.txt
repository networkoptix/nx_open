set(product.title "${client.name}")
set(product.display.title "${client.display.name}")

if(WINDOWS)
    set(nov_file_launcher_executable "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/launcher${CMAKE_EXECUTABLE_SUFFIX}")
else()
    set(nov_file_launcher_executable)
endif()

nx_find_files(ui_files src "ui")
qt5_wrap_ui(uic_files ${ui_files})

nx_configure_file(
    maven/filter-resources/resources/globals.json
    ${CMAKE_CURRENT_BINARY_DIR})

nx_configure_file(
    maven/filter-resources/client_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

if(MACOSX)
    set(mac_sources src/ui/workaround/mac_utils.mm)
endif()

file(GLOB_RECURSE QML_FILES static-resources/src/qml/*.qml static-resources/src/qml/*.js)

nx_add_target(nx_client_desktop LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/client_app_info_impl.cpp"
        ${uic_files}
        ${mac_sources}
    ADDITIONAL_RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/skins/${defaultSkin}"
        "${customization_dir}/nx_client_desktop/resources"
        "${customization_dir}/icons/all/logo.png"
        "${customization_dir}/license.html"
        "${CMAKE_CURRENT_BINARY_DIR}/globals.json"
        "${nov_file_launcher_executable}"
    OTHER_SOURCES
        ${QML_FILES}
    PUBLIC_LIBS
        Qt5::Widgets Qt5::OpenGL Qt5::WebKit Qt5::WebKitWidgets
        Qt5::Qml Qt5::QuickWidgets
        nx_client_core nx_speech_synthesizer nx_audio libvms_gateway
)

if(WINDOWS)
    add_dependencies(nx_client_desktop nov_file_launcher)
endif()

target_include_directories(nx_client_desktop
    PRIVATE
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${Qt5Widgets_PRIVATE_INCLUDE_DIRS})

if(LINUX)
    target_link_libraries(nx_client_desktop PRIVATE Qt5::X11Extras Xss Xfixes X11)
endif()

if(WINDOWS)
    target_link_libraries(nx_client_desktop PRIVATE d3d9 Winmm)
endif()
