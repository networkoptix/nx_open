## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(build_shared_libs ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "") #< We always need static gtest.

# Prevent overriding the parent project's compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GMOCK OFF CACHE INTERNAL "")
set(INSTALL_GTEST OFF CACHE INTERNAL "")

if(MACOSX)
    # This LLVM optimization causes random crashes in the gtest code (primarily in testcases
    # destruction).
    add_compile_options("SHELL:-mllvm -disable-machine-cse")
endif()

add_subdirectory(
    ${open_source_root}/artifacts/googletest
    ${CMAKE_BINARY_DIR}/artifacts/googletest
)

add_library(GTest INTERFACE)
target_link_libraries(GTest INTERFACE gtest gtest_main)
add_library(GMock INTERFACE)
target_link_libraries(GMock INTERFACE gmock gmock_main)

set_target_properties(gtest gtest_main gmock gmock_main PROPERTIES FOLDER third_party)

set(BUILD_SHARED_LIBS ${build_shared_libs} CACHE INTERNAL "")
