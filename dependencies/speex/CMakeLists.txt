## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(speex_dir ${open_source_root}/artifacts/speex/src)

file(GLOB sources CONFIGURE_DEPENDS "${speex_dir}/speex/*.cpp")
file(GLOB headers CONFIGURE_DEPENDS "${speex_dir}/speex/*.h")

add_library(speex STATIC ${headers} ${sources})
target_link_libraries(speex PRIVATE ffmpeg::ffmpeg)

set_target_properties(speex PROPERTIES FOLDER third_party)

target_include_directories(speex PUBLIC ${speex_dir})
