## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_executable(nx_sdk_ut
    src/ref_countable_ut.cpp
    src/ptr_ut.cpp
    src/uuid_helper_ut.cpp
    src/main.cpp
)

target_link_libraries(nx_sdk_ut
    nx_kit
    nx_sdk
)

target_compile_definitions(nx_sdk_ut PRIVATE NX_PLUGIN_API=)

add_test(NAME nx_sdk_ut COMMAND nx_sdk_ut)
set_target_properties(nx_sdk_ut PROPERTIES FOLDER sdk)
