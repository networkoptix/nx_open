cmake_minimum_required(VERSION 3.3)
project(proxy_decoder)

# ATTENTION: This cmake file is not intended for building but rather just for IDE convenience.

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DNX_UTILS_API= -DPROXY_DECODER_API=")

set(SOURCE_FILES
    proxy_decoder.h
    proxy_decoder.cpp
    vdpau_utils.h
    vdpau_utils.cpp
    vdpau_session.h
    vdpau_session.cpp
    proxy_decoder_utils.h
    proxy_decoder_utils.cpp
    proxy_decoder_impl.cpp
    proxy_decoder_stub.cpp
    ../../../buildenv/packages/any/nx_kit/src/nx/kit/ini_config.cpp
    ../../../buildenv/packages/any/nx_kit/src/nx/kit/debug.cpp
)

# Directory containing all necessary headers from the libraries used.
include_directories(
    ../../../bpi/src/include
    ../../../buildenv/packages/any/nx_kit/src
)

add_executable(proxy_decoder ${SOURCE_FILES})
