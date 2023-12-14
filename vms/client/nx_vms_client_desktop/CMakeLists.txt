## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6
    COMPONENTS
        Widgets
        OpenGL
        OpenGLWidgets
        QuickWidgets
        QuickControls2
        QuickTemplates2
        WebEngineQuick
        WebEngineWidgets
        Test
        ShaderTools
    REQUIRED)

find_package(hidapi REQUIRED)

find_package(range-v3 REQUIRED)

if(WINDOWS)
    set(nov_file_launcher_executable "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/launcher${CMAKE_EXECUTABLE_SUFFIX}")
else()
    set(nov_file_launcher_executable)
endif()

nx_find_files(ui_files src "ui")
qt6_wrap_ui(uic_files ${ui_files})

if(MACOSX)
    set(mac_sources src/ui/workaround/mac_utils.mm)
endif()

set(nx_vms_client_desktop_external_resources_file ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/client_external.dat)
nx_add_external_resources_target(nx_vms_client_desktop_external_resources
    TARGET_FILE
        ${nx_vms_client_desktop_external_resources_file}
    QRC_FILE
        client_external
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/external_resources
        ${CMAKE_CURRENT_LIST_DIR}/skins/${customization.skin}
        ${customization_dir}/nx_client_desktop/resources
        ${customization_dir}/icons/all/logo.png
)

# Place Roboto font into resources so it can be loaded by Qt WebEngine.
set(client_font_roboto_regular ${CONAN_ROBOTO-FONTS_ROOT}/bin/fonts/Roboto-Regular.ttf)
set_source_files_properties(${client_font_roboto_regular} PROPERTIES RESOURCE_ALIAS_PREFIX fonts/)

file(GLOB_RECURSE QML_FILES CONFIGURE_DEPENDS qml/*)
set_source_files_properties(${QML_FILES}
    PROPERTIES RESOURCE_BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS shaders/*)
set_source_files_properties(${SHADER_FILES}
    PROPERTIES RESOURCE_BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

set(flowlayout_root "${open_source_root}/artifacts/flowlayout/src")
file(GLOB_RECURSE flowlayout_sources "${flowlayout_root}/flowlayout/*")

set(qt_graphics_items_root "${open_source_root}/artifacts/qt_graphics_items/src")
file(GLOB_RECURSE qt_graphics_items_sources "${qt_graphics_items_root}/qt_graphics_items/*")
file(GLOB_RECURSE qt_graphics_items_headers "${qt_graphics_items_root}/qt_graphics_items/*.h")

set(qt_styles_root "${open_source_root}/artifacts/qt_styles/src")
file(GLOB_RECURSE qt_styles_sources "${qt_styles_root}/qt_styles/*")
file(GLOB_RECURSE qt_styles_headers "${qt_styles_root}/qt_styles/*.h")

set(qt_widgets_root "${open_source_root}/artifacts/qt_widgets/src")
file(GLOB_RECURSE qt_widgets_sources "${qt_widgets_root}/qt_widgets/*")
file(GLOB_RECURSE qt_widgets_headers "${qt_widgets_root}/qt_widgets/*.h")

set(qt_images_root "${open_source_root}/artifacts/qt_images/src")
nx_find_files(qt_images_sources "${qt_images_root}/qt_images" "cpp")
nx_find_files(qt_images_headers "${qt_images_root}/qt_images" "h")

# The directory "update_verification_keys/" contains public key files for verifying
# signatures of the Client and Server update packages downloaded by the update installer.
set(update_verification_keys ${open_source_root}/vms/static-resources/update_verification_keys)
nx_set_qrc_source_files_properties(${update_verification_keys}
    ALIAS_PREFIX update_verification_keys/)

nx_add_target(nx_vms_client_desktop LIBRARY
    ADDITIONAL_SOURCES
        ${uic_files}
        ${mac_sources}
        ${flowlayout_sources}
        ${qt_graphics_items_sources}
        ${qt_styles_sources}
        ${qt_widgets_sources}
        ${qt_images_sources}
    ADDITIONAL_RESOURCES
        "${customization_dir}/license.html"
        "${update_verification_keys}"
        "${nov_file_launcher_executable}"
        "${client_font_roboto_regular}"
        "${QML_FILES}"
        "${SHADER_FILES}"
    ADDITIONAL_MOCABLES
        src/nx/vms/client/desktop/system_administration/widgets/analytics_settings_widget.cpp
        src/nx/vms/client/desktop/ui/common/global_tool_tip.cpp
    ADDITIONAL_MOCABLES
        ${qt_graphics_items_headers}
        ${qt_styles_headers}
        ${qt_widgets_headers}
        ${qt_imagess_headers}
    OTHER_SOURCES
        ${QML_FILES}
        ${SHADER_FILES}
    PUBLIC_LIBS
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGLWidgets Qt6::Network
        Qt6::Qml Qt6::QuickWidgets Qt6::QuickControls2 Qt6::QuickTemplates2
        Qt6::Xml Qt6::Multimedia Qt6::Concurrent
        Qt6::WebEngineQuick
        Qt6::WebEngineWidgets
        nx_vms_client_core
        nx_speech_synthesizer
        nx_audio
        vms_gateway_core
        nx_vms_applauncher_api
        nx_vms_update
        qthttpserver
        nx_kit
        nx_monitoring
        nx_pathkit
        hidapi::hidapi
        range-v3::range-v3
    FOLDER client/libs
    GDI
    MACOS_ARG_MAX_WORKAROUND
)
nx_make_target_translatable(nx_vms_client_desktop)
add_dependencies(nx_vms_client_desktop nx_vms_client_desktop_external_resources)

file(GLOB_RECURSE shader_sources CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "qml/*.vert"
    "qml/*.frag"
    "shaders/qsb/*.vert"
    "shaders/qsb/*.frag"
    "shaders/rhi/media_output/*.vert"
    #"shaders/rhi/media_output/*.frag" - there is a single fragment shader that requires special handling.
)

# This command generates nx_vms_client_desktop_other_files target.
qt6_add_shaders(nx_vms_client_desktop "nx_vms_client_desktop_shaders"
    BATCHABLE
    PREFIX
        "/"
    FILES
        ${shader_sources}
)
set_property(TARGET nx_vms_client_desktop_other_files PROPERTY FOLDER client/other_files)

set(formats "nv12;rgb;yv12;yva12")
set(color_corrections "yuv_gamma;no_gamma")
set(camera_projections "equidistant;stereographic;equisolid;equirectangular360")
set(view_projections "rectilinear;equirectangular")

macro(add_frag_shader unique_shader_suffix shader_defines)
    qt6_add_shaders(nx_vms_client_desktop "desktop_client_fragment_shaders_${unique_shader_suffix}"
        BATCHABLE
        PREFIX
            "/"
        FILES
            "shaders/rhi/media_output/main.frag"
        DEFINES
            ${shader_defines}
        OUTPUTS
            "shaders/rhi/media_output/main_${unique_shader_suffix}.frag.qsb"
    )
endmacro()

foreach(format ${formats})
    foreach(color ${color_corrections})
        # Dewarping
        foreach(camera_projection ${camera_projections})
            foreach(view_projection ${view_projections})
                add_frag_shader(
                    "${format}_${color}_${camera_projection}_${view_projection}"
                    "format_${format}=1;${color}=1;dewarping=1;camera_${camera_projection};view_${view_projection}")
            endforeach()
        endforeach()
        # No texture transformation
        add_frag_shader("${format}_${color}" "format_${format}=1;${color}=1")
    endforeach()
endforeach()

target_compile_definitions(nx_vms_client_desktop
    PRIVATE NX_VMS_CLIENT_DESKTOP_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_CLIENT_DESKTOP_API=${API_IMPORT_MACRO})

target_include_directories(nx_vms_client_desktop
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE ${flowlayout_root}
    PRIVATE ${qt_graphics_items_root}
    PRIVATE ${qt_styles_root}
    PRIVATE ${qt_widgets_root}
    PRIVATE ${qt_images_root}
)

if(WINDOWS)
    add_dependencies(nx_vms_client_desktop nov_file_launcher)
endif()

target_include_directories(nx_vms_client_desktop
    PRIVATE
        ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
        ${Qt6Widgets_PRIVATE_INCLUDE_DIRS}
        ${Qt6WebEngine_PRIVATE_INCLUDE_DIRS}
        ${Qt6WebEngineQuick_PRIVATE_INCLUDE_DIRS}
        ${Qt6QuickTemplates2_PRIVATE_INCLUDE_DIRS}
)

if(LINUX)
    target_link_libraries(nx_vms_client_desktop
        PRIVATE
            Xss
            Xfixes
            X11
            GL
            GLU
            pthread)
endif()

if(WINDOWS)
    target_link_libraries(nx_vms_client_desktop PRIVATE d3d9 Winmm Dinput8 dxguid)
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
