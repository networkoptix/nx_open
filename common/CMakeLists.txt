cmake_minimum_required(VERSION 2.8.11)

# Default configuration values. These must be before the project command or
# they won't work in Windows.
# If no build type is specified, default to "Release"

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Release Debug RelWithDebInfo"
        FORCE)
endif()
# Install to "dist" directory in Windows for testing and as a staging directory
# for the installer.
if(WIN32 AND NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX dist CACHE STRING "Install path prefix.")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/defines.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/defines.h)

include(default_properties)
message(${CMAKE_PREFIX_PATH})

project(common)
set(PROJECT_SHORTNAME "common")
set(PROJECT_LONGNAME "${company.name} Common Library")
set(PROJECT_VERSION "2.6.0")

include(default_project_properties)
# Additional Include Directories
include_directories($ENV{environment}/boost_1_56_0
                    src
                    ${CMAKE_SOURCE_DIR}/common/src
                    $ENV{environment}/include
                    #$ENV{environment}/include/ffmpeg-misc-headers-win32
                    $ENV{environment}/include/glext
                    $ENV{environment}/qt5-custom
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_utils/src
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_network/src
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_streaming/src
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_media/src
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_audio/src
                    $ENV{environment}/qt5-custom
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtZlib
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtCore/${qt.version}/QtCore
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtCore/${qt.version}
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtCore
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtQuick/${qt.version}/QtQuick
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtQuick/${qt.version}
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtQuick
                    $ENV{environment}/artifacts/openal/${openal.version}/include
                    $ENV{environment}/artifacts/openal/${openal.version}/include/AL
                    $ENV{environment}/artifacts/directx/${directx.version}/include
                    $ENV{environment}/artifacts/festival/${festival.version}/include
                    $ENV{environment}/artifacts/ffmpeg/${ffmpeg.version}/include
                    $ENV{environment}/artifacts/gmock/${gmock.version}/include
                    $ENV{environment}/artifacts/gtest/${gtest.version}/include
                    $ENV{environment}/artifacts/mfx/${mfx.version}/include
                    $ENV{environment}/artifacts/onvif/${onvif.version}/include
                    $ENV{environment}/artifacts/openssl/${openssl.version}/include
                    $ENV{environment}/artifacts/quazip/${quazip.version}/include
                    $ENV{environment}/artifacts/sigar/${sigar.version}/include
                    $ENV{environment}/artifacts/vmax_integration/${vmax_integration.version}/include
                    )

# Global CMake options
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_definitions(-DUSE_NX_HTTP
                -DNOMINMAX=
                -D__STDC_CONSTANT_MACROS
                -D_USING_V110_SDK71_
                -DENABLE_VMAX
                -DQN_EXPORT=
                -DUNICODE
                -DENABLE_ONVIF
                -DENABLE_AXIS
                -DENABLE_ACTI
                -DENABLE_ARECONT
                -DENABLE_DLINK
                -DENABLE_DROID
                -DENABLE_TEST_CAMERA
                -DENABLE_STARDOT
                -DENABLE_IQE
                -DENABLE_ISD
                -DENABLE_PULSE_CAMERA
                -DENABLE_COLDSTORE
                -DENABLE_MDNS
                -DENABLE_ARCHIVE
                -DENABLE_THIRD_PARTY
                -DENABLE_SOFTWARE_MOTION_DETECTION
                -DENABLE_DESKTOP_CAMERA
                -DENABLE_DATA_PROVIDERS
                -DENABLE_SENDMAIL
                -DENABLE_SSL
                -DUSE_OWN_MUTEX
                -DNX_NETWORK_API=__declspec\(dllimport\)
                -DNX_UTILS_API=__declspec\(dllimport\)
                -DUDT_API=__declspec\(dllimport\)
                )

if(NOT SKIP_GEN)
    file(GLOB_RECURSE FILTERED_FILES "${PROJECT_SOURCE_DIR}/resources/filter/*")
    foreach(filename ${FILTERED_FILES})
        string(REPLACE "${PROJECT_SOURCE_DIR}/resources/filter/" "" filename ${filename})
        configure_file(${PROJECT_SOURCE_DIR}/resources/filter/${filename}
                       ${CMAKE_CURRENT_BINARY_DIR}/${filename})
        message("Filtering and Copying: " ${filename})
    endforeach(filename ${FILTERED_FILES})

    file(GLOB_RECURSE COMMON_FILTERED_FILES "${CMAKE_SOURCE_DIR}/cpp/resources/filter/*")
    foreach(filename ${COMMON_FILTERED_FILES})
        string(REPLACE "${CMAKE_SOURCE_DIR}/cpp/resources/filter/" "" filename ${filename})
        configure_file(${CMAKE_SOURCE_DIR}/cpp/resources/filter/${filename}
                    ${CMAKE_CURRENT_BINARY_DIR}/${filename})
        message("Filtering and Copying: " ${filename})
    endforeach(filename ${COMMON_FILTERED_FILES})

    #configure_file(${CMAKE_SOURCE_DIR}/cpp/scripts/gen_resources.py
    #               ${CMAKE_CURRENT_BINARY_DIR}/gen_resources.py)
    #file(GLOB RESOURCES "${PROJECT_SOURCE_DIR}/resources/filter/*")
    file(COPY ${RESOURCES} DESTINATION ${PROJECT_BINARY_DIR})
    execute_process(
        COMMAND $ENV{environment}/python/x64/python.exe ${CMAKE_CURRENT_BINARY_DIR}/gen_resources.py
    )
endif()

# Configure Qt
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5XmlPatterns REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Multimedia REQUIRED)
find_package(Qt5LinguistTools)
find_package(Qt5Widgets)

# Windows application icon
#if(WIN32)
  #set(WINDOWS_RES_FILE ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.obj)
  #if(MSVC)
  #  add_custom_command(OUTPUT ${WINDOWS_RES_FILE}
  #    COMMAND rc.exe /fo ${WINDOWS_RES_FILE} ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.rc
  #    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/win
  #  )
  #else()
  #  add_custom_command(OUTPUT ${WINDOWS_RES_FILE}
  #    COMMAND windres.exe ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME} ${WINDOWS_RES_FILE}
  #    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/win
  #  )
  #endif()
#endif()

if(NOT MSVC)
    # Enable the C++11 standard
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}
        -std=c++11
        -Werror=enum-compare
        -Werror=reorder
        -Werror=delete-non-virtual-dtor
        -Werror=return-type
        -Werror=conversion-null
        -Wuninitialized)
endif()

find_sources(src)

file(GLOB_RECURSE ALL_TRANSLATION_FILES "${PROJECT_SOURCE_DIR}/translations/*")
foreach(filename ${ALL_TRANSLATION_FILES})
    foreach(translation ${translations})
        if(filename MATCHES "${translation}.ts")
            get_filename_component(NAME ${filename} NAME)
            get_filename_component(PATH ${filename} PATH)
            list(APPEND TRANSLATION_FILES ${filename})
            #list(APPEND QM_FILES ${PATH}/${NAME}.qm)
        endif()
    endforeach(${translation})
endforeach(filename ${ALL_TRANSLATION_FILES})

foreach(filename ${TRANSLATION_FILES})
    message("Adding translation: " ${filename})
endforeach(filename ${TRANSLATION_FILES})

QT5_ADD_TRANSLATION(QM_FILES ${TRANSLATION_FILES})

foreach(filename ${QM_FILES})
    message("Adding translation: " ${filename})
endforeach(filename ${QM_FILES})

qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

LINK_DIRECTORIES($ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}
                 $ENV{environment}/artifacts/openal/${openal.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/directx/${directx.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/festival/${festival.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/ffmpeg/${ffmpeg.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/gmock/${gmock.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/gtest/${gtest.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/mfx/${mfx.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/onvif/${onvif.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/openssl/${openssl.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/quazip/${quazip.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/sigar/${sigar.version}/${platform}/${box}/${arch}/lib/${configuration}
                 $ENV{environment}/artifacts/vmax_integration/${vmax_integration.version}/${platform}/${box}/${arch}/lib/${configuration}
                 )

add_library(${PROJECT_SHORTNAME} STATIC
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
  ${SETDDM}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Qt5::Core Qt5::Gui Qt5::Network Ws2_32.lib glu32.lib opengl32.lib odbccp32.lib
)

set_target_properties(${PROJECT_SHORTNAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/stdafx.h")
cotire(${PROJECT_SHORTNAME})

if(UNIX)
    install(TARGETS ${PROJECT_SHORTNAME}
            RUNTIME DESTINATION bin)
elseif(WIN32)
    install(TARGETS ${PROJECT_SHORTNAME}
            DESTINATION .)
endif()
