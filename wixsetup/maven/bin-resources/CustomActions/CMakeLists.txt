cmake_minimum_required(VERSION 2.8.12)

list(APPEND CMAKE_MODULE_PATH
    ${ROOT_DIR}/build_utils/cmake
    ${ROOT_DIR}/cmake)
include(find_sources)
include(precompiled_header)

if(NOT CONFIG_DIRECTORY)
    message(FATAL_ERROR "CONFIG_DIRECTORY is not specified.")
endif()

#add_definitions(-DUNICODE -D_CONSOLE -D_UNICODE)
include_directories("${CONFIG_DIRECTORY}" "${CMAKE_CURRENT_SOURCE_DIR}" "$ENV{environment}/wix/sdk/VS2015/inc")
find_sources("${CMAKE_CURRENT_SOURCE_DIR}" cpp_files hpp_files)
link_directories("$ENV{environment}/wix/sdk/VS2015/lib/x64")
add_library(customactions SHARED ${cpp_files} ${hpp_files})
add_precompiled_header(customactions "${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h" ${pch_flags})
set_target_properties(customactions PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
target_link_libraries(customactions userenv.lib;Rpcrt4.lib;Ws2_32.lib;IPHLPAPI.lib;comsuppw.lib;msi.lib;dutil.lib;wcautil.lib;Version.lib)

install(TARGETS customactions RUNTIME DESTINATION bin/Release)
