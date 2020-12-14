cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

include(cmake/utils.cmake)
nx_init_known_files_list()
include(cmake/option.cmake)
include(cmake/default_target.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(customization "default" CACHE STRING "Product customization")
set(publicationType "local" CACHE STRING "Product publication type")
set(buildNumber "0" CACHE STRING "Build number")
set(eulaVersionOverride "" CACHE STRING "Overriden EULA version")

# Remember to reset the in-version protocol number in (cmake/vms_protocol_version.cmake) when
# updating the project version.
set(releaseVersion 4.2.0)

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

# Setup ninja_tool. Substitute CMAKE_MAKE_PROGRAM value with custom launcher and perform
# initialization of ninja_tool-related data.
include(cmake/ninja_tool.cmake)
nx_setup_ninja_preprocessor()

include(cmake/vcs_helpers.cmake)
nx_vcs_get_info(CHANGESET changeSet BRANCH branch CURRENT_REFS currentRefs)
if(NOT changeSet OR NOT branch OR NOT currentRefs)
    message(FATAL_ERROR "VCS info cannot be read.")
endif()

include(cmake/dependencies.cmake)
# Load generated dependencies.cmake, containing packages path variables.
load_generated_dependencies_file()

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
include(code_signing)
include(target_detection)
include(find_sources)
include(moc_utils)
include(qrc_utils)
include(cpp_utils)
include(strip_target)
include(vms_protocol_version)
include(customization_package)

set(metaVersion H4)
if(customization.advanced.useMetaVersion)
    # Set meta-version to be shown in MetaVMS builds - in the Client, etc.
    set(usedMetaVersion ${metaVersion})
endif()

# Construct nxSdkVersion.
set(nxSdkVersion "${releaseVersion} ${metaVersion}")
if(nxSdkVersion MATCHES "^ " OR nxSdkVersion MATCHES " $")
    message(FATAL_ERROR "INTERNAL ERROR: Either releaseVersion or metaVersion not set.")
endif()
if(developerBuild)
    string(APPEND nxSdkVersion " dev")
    if (NOT "${changeSet}" STREQUAL "")
        string(APPEND nxSdkVersion " ${changeSet}")
    endif()
endif()

include(properties)
include(cloud_host)
include(vms_parts)
include(compiler_options)
include(output_directories)
include(distribution_names)
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
        Sql
        Concurrent
        Multimedia
        Qml
)

if(withDesktopClient OR withMobileClient)
    find_package(Qt5 COMPONENTS Quick)
endif()

if(withDesktopClient)
    find_package(Qt5 COMPONENTS Widgets)
endif()

set(QML_IMPORT_PATH
    "${QT_DIR}/qml"
    "${CMAKE_CURRENT_LIST_DIR}/vms/client/nx_vms_client_desktop/static-resources/qml"
    "${CMAKE_CURRENT_LIST_DIR}/vms/client/mobile_client/static-resources/qml"
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

add_subdirectory(vms)

if(withTests)
    add_subdirectory(tests)
endif()

include(fix_qt_vars)

if(withClouds)
    add_custom_target(cloud DEPENDS cloud-backend)
    set_target_properties(cloud PROPERTIES FOLDER cloud)

    if(LINUX)
        add_subdirectory(cloud_portal)
        add_custom_target(cloud-frontend DEPENDS cloud_portal)
        set_target_properties(cloud-frontend PROPERTIES FOLDER cloud)
        add_dependencies(cloud cloud-frontend)
    endif()
endif()

if(withDocumentation)
    add_subdirectory(doc)
endif()

nx_save_known_files()

nx_add_pre_build_artifacts_check()
nx_use_custom_verify_globs()
nx_save_ninja_preprocessor_script()
