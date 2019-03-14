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

add_test(NAME nx_sdk_ut COMMAND nx_sdk_ut)
