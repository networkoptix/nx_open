cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

include(cmake/option.cmake)
include(cmake/default_target.cmake)

set(customization "default" CACHE STRING "Product customization")
nx_option(beta "Is this build is beta build" ON)
set(buildNumber "0" CACHE STRING "Build number")

set(releaseVersion 4.0.0)

nx_option(developerBuild
    "Disable this option for production builds (Jenkins)"
    ON)

if(developerBuild)
    set(default_cmake_build_type "Debug")
else()
    if(targetDevice MATCHES "edge1")
        set(default_cmake_build_type "MinSizeRel")
    else()
        set(default_cmake_build_type "Release")
    endif()
endif()

set(CMAKE_BUILD_TYPE ${default_cmake_build_type} CACHE STRING
    "One of [Debug Release RelWithDebInfo MinSizeRel].")
set(CMAKE_VS_PLATFORM_TOOLSET_HOST_ARCHITECTURE "x64" CACHE INTERNAL "")

set(targetDevice ${default_target_device} CACHE STRING
    "Target device. For the list see cmake/toolchains.")
set(box "none")

if(targetDevice STREQUAL "none")
    set(targetDevice "")
endif()

if(NOT CMAKE_TOOLCHAIN_FILE AND targetDevice)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain/${targetDevice}.cmake)
    if(NOT EXISTS ${CMAKE_TOOLCHAIN_FILE})
        unset(CMAKE_TOOLCHAIN_FILE)
    endif()
endif()

# Early loading rdep to download a toolchain if needed.
include(cmake/find_python.cmake)
include(cmake/rdep.cmake)
nx_rdep_configure()

include(cmake/mercurial.cmake)
hg_changeset(${CMAKE_CURRENT_LIST_DIR} changeSet)
hg_branch(${CMAKE_CURRENT_LIST_DIR} branch)

include(cmake/dependencies.cmake)

project(vms VERSION ${releaseVersion})

if(targetDevice STREQUAL "ios")
    include(cmake/ios.cmake)
endif()

set(releaseVersion.short ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set(releaseVersion.full ${releaseVersion}.${buildNumber})

set(mobileClientVersion 18.5)
set(mobileClientVersion.full ${mobileClientVersion}.${buildNumber})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_DEBUG_POSTFIX "" CACHE INTERNAL "" FORCE)

list(APPEND CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/cmake
)

include(CMakeParseArguments)
include(CMakeDependentOption)
include(ExternalProject)

nx_option(codeSigning "Enable code signing" OFF)

include(target_detection)
include(utils)
include(find_sources)
include(moc_utils)
include(qrc_utils)
include(precompiled_header)
include(strip_target)
include(ec_protocol)

set(customization_dir "${PROJECT_SOURCE_DIR}/customization/${customization}")

include(customization/default-values.cmake)
include(${customization_dir}/customization.cmake)
include(properties)
include(cloud_host)
include(vms_parts)
include(compiler_options)
include(output_directories)
include(distribution_names)

if(WINDOWS)
    include(windows_signing)
endif()

include(parameters_validation)

if(WINDOWS OR ANDROID OR IOS)
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
else()
    set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")
endif()

load_dependencies()
include(maven_compatibility)
include(generate_current_config)

if(MACOSX OR IOS)
    include(macosx_keychain)
endif()

include(project_utils)
include(test_utils)

nx_create_dev_qt_conf()

list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})
if(MACOSX)
    set(CMAKE_FRAMEWORK_PATH "${QT_DIR}/lib")
    list(APPEND CMAKE_INSTALL_RPATH ${QT_DIR}/lib)
endif()
if(LINUX)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${QT_DIR}/lib")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath-link,${QT_DIR}/lib")
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

if(ANDROID)
    include(android-apk)
elseif(IOS)
    include(ios_app)
endif()

link_directories("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

find_package(Qt5
    COMPONENTS
        LinguistTools
        Core
        Gui
        Network
        Xml
        XmlPatterns
        Sql
        Concurrent
        Multimedia
        Qml
)

if(withDesktopClient OR withMobileClient OR withNxTool)
    find_package(Qt5 COMPONENTS Quick)
endif()

if(withDesktopClient OR withNxTool)
    find_package(Qt5 COMPONENTS Widgets)
endif()

set(QML_IMPORT_PATH
    "${QT_DIR}/qml"
    "${CMAKE_CURRENT_LIST_DIR}/vms/client/nx_vms_client_desktop/static-resources/qml"
    "${CMAKE_CURRENT_LIST_DIR}/vms/client/mobile_client/static-resources/qml"
    "${CMAKE_CURRENT_LIST_DIR}/nxtool/static-resources/src/qml"
    CACHE STRING "Extra QML import paths")

if(ANDROID)
    find_package(Qt5 COMPONENTS AndroidExtras)
endif()

if(WINDOWS)
    # Prepare a file to be included into each Visual Studio project.
    nx_configure_file(${PROJECT_SOURCE_DIR}/msvc.user.props ${CMAKE_BINARY_DIR})
endif()

list(APPEND CMAKE_MSVCIDE_RUN_PATH ${QT_DIR}/bin)

add_subdirectory(dependencies)

if (withMediaServer OR withTestCamera OR withTrayTool OR withDesktopClient OR withMobileClient OR withClouds)
    add_subdirectory(libs)
    add_subdirectory(cloud)
endif()

#add_subdirectory(_axis_controller)
#add_subdirectory(_dw_vca_controller)
#add_subdirectory(_dw_mtt_controller)

add_subdirectory(doc)
add_subdirectory(vms)

if(withTests)
    add_subdirectory(tests)
endif()

include(fix_qt_vars)

if(beta)
    set(beta_bool "true")
else()
    set(beta_bool "false")
endif()

if(withClouds)
    add_custom_target(cloud DEPENDS cloud-backend)

    if(LINUX)
        add_subdirectory(cloud_portal)
        add_custom_target(cloud-frontend DEPENDS cloud_portal)
        add_dependencies(cloud cloud-frontend)
    endif()
endif()

nx_save_known_files()
