## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(qthttpserver_dir ${open_source_root}/artifacts/qthttpserver)

file(GLOB sources CONFIGURE_DEPENDS "${qthttpserver_dir}/*.cpp")
file(GLOB headers CONFIGURE_DEPENDS "${qthttpserver_dir}/*.h")
qt6_wrap_cpp(moc_sources ${headers})

add_library(qthttpserver STATIC
    ${headers} ${sources} ${moc_sources}
)
set_target_properties(qthttpserver PROPERTIES FOLDER third_party)

target_link_libraries(qthttpserver
    PUBLIC
        Qt6::Core Qt6::Network
)

target_include_directories(qthttpserver
    PUBLIC
        ${qthttpserver_dir}
)
