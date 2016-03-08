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

project(common)
set(PROJECT_SHORTNAME "common")
set(PROJECT_LONGNAME "${company.name} Common Library")
set(PROJECT_VERSION "2.6.0")

include(default_project_properties)

# Additional Include Directories
# Global CMake options
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
#set (CMAKE_AUTOMOC_MOC_OPTIONS "-bstdafx.h" "-fstdafx.h")

# Additional Include Directories
include_directories (${include_directories}
                    src
                    )

# Configure Qt
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5XmlPatterns REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Multimedia REQUIRED)
find_package(Qt5Widgets)

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
process_resources()
add_translations(translations)
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

#MACRO(ADD_MSVC_PRECOMPILED_HEADER PrecompiledHeader PrecompiledSource SourcesVar)
#  IF(MSVC)
#    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
#    SET(PrecompiledBinary "$(IntDir)${PrecompiledBasename}.pch")
#    SET(Sources ${${SourcesVar}})

#    SET_SOURCE_FILES_PROPERTIES(${PrecompiledSource}
#                                PROPERTIES COMPILE_FLAGS "/Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
#                                           OBJECT_OUTPUTS "${PrecompiledBinary}")
#    SET_SOURCE_FILES_PROPERTIES(${Sources}
#                                PROPERTIES COMPILE_FLAGS "/Yu\"${PrecompiledHeader}\" /FI\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\""
#                                           OBJECT_DEPENDS "${PrecompiledBinary}")  
    # Add precompiled header to SourcesVar
#    LIST(APPEND ${SourcesVar} ${PrecompiledSource})
#  ENDIF(MSVC)
#ENDMACRO(ADD_MSVC_PRECOMPILED_HEADER)
#if(MSVC)
#  ADD_MSVC_PRECOMPILED_HEADER("src/StdAfx.h" "src/StdAfx.cpp" CPP_FILES)
#endif()
                 
add_library(${PROJECT_SHORTNAME} STATIC
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
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
