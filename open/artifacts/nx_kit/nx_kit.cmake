# Copyright 2017 Network Optix, Inc. Licensed under GNU Lesser General Public License version 3.

set(library_type)
if(WIN32)
    set(library_type SHARED)
endif()

add_library(nx_kit ${library_type}
    ${CMAKE_CURRENT_LIST_DIR}/src/nx/kit/debug.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/nx/kit/ini_config.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/nx/kit/test.cpp
)
target_include_directories(nx_kit
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)

if(WIN32)
    set(NX_KIT_API_IMPORT_MACRO "__declspec(dllimport)")
    set(NX_KIT_API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(NX_KIT_API_IMPORT_MACRO "")
    set(NX_KIT_API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(nx_kit
    PRIVATE NX_KIT_API=${NX_KIT_API_EXPORT_MACRO}
    INTERFACE NX_KIT_API=${NX_KIT_API_IMPORT_MACRO}
)
