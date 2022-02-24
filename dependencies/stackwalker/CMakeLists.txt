## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(stackwalker_dir ${open_source_root}/artifacts/stackwalker/Main/StackWalker)

file(GLOB sources CONFIGURE_DEPENDS ${stackwalker_dir}/*.cpp)

add_definitions(-D_UNICODE)

add_library(stackwalker STATIC ${sources})
set_target_properties(stackwalker PROPERTIES FOLDER third_party)

target_include_directories(stackwalker PUBLIC ${stackwalker_dir})
