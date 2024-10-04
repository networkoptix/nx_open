## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_codec
    LIBRARY
    PUBLIC_LIBS
        nx_utils
        ffmpeg::ffmpeg # TODO remove dependency
    FOLDER common/libs
)

target_compile_definitions(nx_codec
    PRIVATE NX_CODEC_API=${API_EXPORT_MACRO}
    INTERFACE NX_CODEC_API=${API_IMPORT_MACRO}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
