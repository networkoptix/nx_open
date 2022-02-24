## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(detours_dir ${open_source_root}/artifacts/detours/src)

file(GLOB sources CONFIGURE_DEPENDS ${detours_dir}/*.cpp)
list(REMOVE_ITEM sources ${detours_dir}/uimports.cpp)

add_library(detours STATIC ${sources})
set_target_properties(detours PROPERTIES FOLDER third_party)

target_include_directories(detours PUBLIC ${detours_dir})
