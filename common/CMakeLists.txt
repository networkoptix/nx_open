project(common)
set(PROJECT_SHORTNAME "common")
set(PROJECT_LONGNAME "${company.name} Common Library")

include(default_project_properties)

# Additional Include Directories
include_directories (${include_directories} src)

# Configure Qt
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5XmlPatterns REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Multimedia REQUIRED)

find_sources(src)
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

get_external_dependencies(openssl quazip)

if(${CMAKE_BUILD_TYPE} MATCHES Debug)
   include(${CMAKE_CURRENT_BINARY_DIR}/dependencies-debug.cmake)
else()
   include(${CMAKE_CURRENT_BINARY_DIR}/dependencies.cmake)
endif()

include_directories(${OPENSSL_INCLUDE}
                    ${QUAZIP_INCLUDE}
)

add_library(${PROJECT_SHORTNAME} ${DEFAULT_LIBRARY_TYPE}
            ${UI_HEADERS}
            ${CPP_FILES}
            ${HEADER_FILES}
            ${RESOURCE_FILES}
)

target_link_libraries(${PROJECT_SHORTNAME}
                      nx_utils
                      nx_streaming
                      Qt5::Core
                      Qt5::Gui
                      Qt5::Network
                      Qt5::Xml
                      Qt5::XmlPatterns
                      Qt5::Sql
                      Qt5::Concurrent
                      Qt5::Multimedia
                      ws2_32
                      Iphlpapi
                      Ole32
                      sigar
                      Secur32
                      avcodec
                      avdevice
                      avfilter
                      avformat
                      avutil
                      swscale
                      postproc
                      ${OPENSSL_LIB}
                      ${QUAZIP_LIB}
)

if (UNIX)
    install(TARGETS ${PROJECT_SHORTNAME}
            LIBRARY DESTINATION lib)
elseif (WIN32)
    install(TARGETS ${PROJECT_SHORTNAME}
            DESTINATION .)
endif()
