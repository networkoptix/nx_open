## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(udt_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/udt)

nx_add_target(udt LIBRARY NO_MOC WERROR_IF LINUX
    SOURCE_DIR ${udt_dir}/src
    FOLDER common/libs
)

target_compile_definitions(udt
    PRIVATE UDT_API=${API_EXPORT_MACRO}
    INTERFACE UDT_API=${API_IMPORT_MACRO})

if(WINDOWS)
    target_link_libraries(udt PUBLIC ws2_32)
endif()

if(LINUX AND NOT ANDROID)
    target_link_libraries(udt PRIVATE rt pthread)
endif()

if(withTests)
    add_subdirectory(${udt_dir}/unit_tests ${CMAKE_BINARY_DIR}/open/artifacts/udt)
endif()
