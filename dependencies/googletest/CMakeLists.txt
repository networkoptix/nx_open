set(build_shared_libs ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "") #< We always need static gtest.

# Prevent overriding the parent project's compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GMOCK OFF CACHE INTERNAL "")
set(INSTALL_GTEST OFF CACHE INTERNAL "")

add_subdirectory(
    ${CMAKE_SOURCE_DIR}/artifacts/googletest
    ${CMAKE_BINARY_DIR}/artifacts/googletest
)

add_library(GTest INTERFACE)
target_link_libraries(GTest INTERFACE gtest gtest_main)
add_library(GMock INTERFACE)
target_link_libraries(GMock INTERFACE gmock gmock_main)

set(BUILD_SHARED_LIBS ${build_shared_libs} CACHE INTERNAL "")
