cmake_minimum_required(VERSION 3.3.2)

# Needed for proper RPATH in MacOS.
if(POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
endif()

project(nx_kit_test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(nxKitLibraryType "SHARED" CACHE STRING "nx_kit library type (STATIC or SHARED or empty)")

set (nx_kit_files
    src/nx/kit/utils.h
    src/nx/kit/utils.cpp
    src/nx/kit/debug.h
    src/nx/kit/debug.cpp
    src/nx/kit/ini_config.h
    src/nx/kit/ini_config.cpp
    src/nx/kit/output_redirector.h
    src/nx/kit/output_redirector.cpp
    src/nx/kit/test.h
    src/nx/kit/test.cpp
    src/ini_config_c.h
    src/ini_config_c_impl.h)

if (APPLE)
    set (nx_kit_files ${nx_kit_files}
        src/nx/kit/apple_utils.h
        src/nx/kit/apple_utils.mm)
endif()

add_library(nx_kit ${nxKitLibraryType} ${nx_kit_files})

if (APPLE)
    find_library(Foundation_LIBRARY Foundation REQIRED)
    target_link_libraries(nx_kit ${Foundation_LIBRARY})
endif()

target_include_directories(nx_kit PUBLIC src)

set(NX_KIT_API_IMPORT_MACRO "")
set(NX_KIT_API_EXPORT_MACRO "")
if(WIN32)
    if (nxKitLibraryType STREQUAL "SHARED")
        set(NX_KIT_API_IMPORT_MACRO "__declspec(dllimport)")
        set(NX_KIT_API_EXPORT_MACRO "__declspec(dllexport)")
    endif()
else()
    set(NX_KIT_API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(nx_kit
    PRIVATE NX_KIT_API=${NX_KIT_API_EXPORT_MACRO}
    INTERFACE NX_KIT_API=${NX_KIT_API_IMPORT_MACRO}
)

# Set the maximum warning level.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /WX)
endif()


set(nxKitWithTests "YES" CACHE STRING "Whether to build unit tests")
if(nxKitWithTests)
    enable_testing()
    add_subdirectory(unit_tests)
endif()
