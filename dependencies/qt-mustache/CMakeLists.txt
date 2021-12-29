## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(qt-mustache_dir ${open_source_root}/artifacts/qt-mustache/src)

file(GLOB sources CONFIGURE_DEPENDS "${qt-mustache_dir}/qt-mustache/*.cpp")
file(GLOB headers CONFIGURE_DEPENDS "${qt-mustache_dir}/qt-mustache/*.h")

add_library(qt-mustache STATIC ${headers} ${sources})
target_link_libraries(qt-mustache PRIVATE Qt6::Core)

set_target_properties(qt-mustache PROPERTIES FOLDER third_party)

target_include_directories(qt-mustache PUBLIC ${qt-mustache_dir})
