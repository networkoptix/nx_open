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

file(GLOB_RECURSE QML_FILES static-resources/qml/*.qml static-resources/qml/*.js)

set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_add_target(nx_vms_client_desktop LIBRARY ${library_type}
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/client_app_info_impl.cpp"
        ${uic_files}
        ${mac_sources}
    ADDITIONAL_RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/skins/${defaultSkin}"
        "${customization_dir}/nx_client_desktop/resources"
        "${customization_dir}/icons/all/logo.png"
        "${customization_dir}/icons/all/videowall.ico"
        "${customization_dir}/license.html"
        "${CMAKE_CURRENT_BINARY_DIR}/globals.json"
        "${nov_file_launcher_executable}"
    ADDITIONAL_MOCABLES
        src/nx/vms/client/desktop/system_administration/widgets/analytics_settings_widget.cpp
        src/nx/vms/client/desktop/ui/common/global_tool_tip.cpp
    OTHER_SOURCES
        ${QML_FILES}
    PUBLIC_LIBS
        Qt5::Core Qt5::Widgets Qt5::Network Qt5::OpenGL Qt5::WebKit Qt5::WebKitWidgets
        Qt5::Qml Qt5::QuickWidgets Qt5::Xml Qt5::Multimedia Qt5::Concurrent
        Qt5::Svg
        nx_vms_client_core
        nx_speech_synthesizer
        nx_audio
        vms_gateway_core
        nx_vms_applauncher_api
        qtsingleapplication
        qthttpserver
        nx_kit
    MACOS_ARG_MAX_WORKAROUND
)

target_compile_definitions(nx_vms_client_desktop
    PRIVATE NX_VMS_CLIENT_DESKTOP_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_CLIENT_DESKTOP_API=${API_IMPORT_MACRO})

if(WINDOWS)
    add_dependencies(nx_vms_client_desktop nov_file_launcher)
endif()

target_include_directories(nx_vms_client_desktop
    PRIVATE
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${Qt5Widgets_PRIVATE_INCLUDE_DIRS})

if(LINUX)
    target_link_libraries(nx_vms_client_desktop PRIVATE Qt5::X11Extras Xss Xfixes X11 GL GLU)
endif()

if(WINDOWS)
    target_link_libraries(nx_vms_client_desktop PRIVATE d3d9 Winmm)
endif()

if(MACOSX)
    find_library(CoreServices_LIBRARY CoreServices)
    mark_as_advanced(CoreServices_LIBRARY)

    find_library(IOKit_LIBRARY IOKit)
    mark_as_advanced(IOKit_LIBRARY)

    find_library(OpenGL_LIBRARY OpenGL)
    mark_as_advanced(OpenGL_LIBRARY)

    target_link_libraries(nx_vms_client_desktop PRIVATE ${CoreServices_LIBRARY} ${IOKit_LIBRARY} ${OpenGL_LIBRARY})
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
