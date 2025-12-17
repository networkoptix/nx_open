## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6 COMPONENTS Multimedia OpenGL REQUIRED)

set(additional_sources)
set(additional_sources_include_dirs)
set(artifacts_dir ${open_source_root}/artifacts)

# Do not enable HW decoders for mobile client on any platform.
if(NOT APPLE AND arch STREQUAL "x64" AND NOT "${productType}" STREQUAL "mobile")
    set(enable_hw_decoders_by_default TRUE)
else()
    set(enable_hw_decoders_by_default FALSE)
endif()

if(APPLE)
    file(GLOB_RECURSE objc_files
        src/nx/media/*.mm
    )
    list(APPEND additional_sources ${objc_files})
endif()

nx_add_target(nx_media
    LIBRARY
    GDI
    PUBLIC_LIBS
        nx_kit
        nx_audio
        nx_vms_common
        nx_fusion
        nx_utils
    PRIVATE_LIBS
        Qt6::Multimedia
        Qt6::OpenGL
    ADDITIONAL_SOURCES
        ${additional_sources}
    FOLDER client/libs
)

target_compile_definitions(nx_media
    PRIVATE NX_MEDIA_API=${API_EXPORT_MACRO}
    INTERFACE NX_MEDIA_API=${API_IMPORT_MACRO}
)

target_include_directories(nx_media
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/include
    PRIVATE
        ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
        ${Qt6Multimedia_PRIVATE_INCLUDE_DIRS}
        ${additional_sources_include_dirs}
)

if(ANDROID OR IOS)
    find_package(JPEG REQUIRED)
    target_link_libraries(nx_media PRIVATE JPEG::JPEG)
endif()

if(MACOSX)
    find_library(CoreVideo_LIBRARY CoreVideo)
    mark_as_advanced(CoreVideo_LIBRARY)
    target_link_libraries(nx_media PRIVATE ${CoreVideo_LIBRARY})

    find_library(CoreMedia_LIBRARY CoreMedia)
    mark_as_advanced(CoreMedia_LIBRARY)
    target_link_libraries(nx_media PRIVATE ${CoreMedia_LIBRARY})

    find_library(VideoToolBox_LIBRARY VideoToolBox)
    mark_as_advanced(VideoToolBox_LIBRARY)
    target_link_libraries(nx_media PRIVATE ${VideoToolBox_LIBRARY})
endif()

if((LINUX AND NOT ANDROID AND arch MATCHES "x64|arm64") OR WINDOWS)
    find_package(VulkanHeaders REQUIRED)
    target_link_libraries(nx_media PRIVATE Vulkan::Headers)
endif()

if(targetDevice STREQUAL "linux_x64")
    find_package(PkgConfig REQUIRED)
    find_package(libva REQUIRED)
    pkg_check_modules(DRM REQUIRED IMPORTED_TARGET libdrm)
    target_link_libraries(nx_media PRIVATE libva::libva X11 dl GL libegl_stub PkgConfig::DRM)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
