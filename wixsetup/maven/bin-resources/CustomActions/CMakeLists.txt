cmake_minimum_required(VERSION 2.8.12)

list(APPEND CMAKE_MODULE_PATH
    ${ROOT_DIR}/build_utils/cmake
    ${ROOT_DIR}/cmake)
include(maven_compatibility)
include(precompiled_header)

if(NOT CONFIG_DIRECTORY)
    message(FATAL_ERROR "CONFIG_DIRECTORY is not specified.")
endif()

set(wixDirectory "" CACHE STRING "Path to WIX SDK root")
if(NOT wixDirectory)
    message(FATAL_ERROR "wixDirectory is not specified.")
endif()

add_definitions(-D_USRDLL -DEVEASSOCCA_EXPORTS -DUNICODE -D_CONSOLE -D_UNICODE)
include_directories("${CONFIG_DIRECTORY}" "${CMAKE_CURRENT_SOURCE_DIR}" "${wixDirectory}/sdk/VS2015/inc")
file(GLOB cpp_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
#find_sources("${CMAKE_CURRENT_SOURCE_DIR}" cpp_files hpp_files)
link_directories("${wixDirectory}/sdk/VS2015/lib/x64")
add_library(customactions SHARED ${cpp_files} "CustomActions.rc")
add_precompiled_header(customactions "${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h")
target_link_libraries(customactions userenv.lib;Rpcrt4.lib;Ws2_32.lib;IPHLPAPI.lib;comsuppw.lib;msi.lib;dutil.lib;wcautil.lib;Version.lib)

install(TARGETS customactions RUNTIME DESTINATION "${CMAKE_BUILD_TYPE}")
