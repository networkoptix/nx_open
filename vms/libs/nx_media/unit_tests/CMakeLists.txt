## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(artifacts_dir ${open_source_root}/artifacts)
list(APPEND additional_sources_include_dirs ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Interface)
list(APPEND additional_sources_include_dirs ${artifacts_dir}/nvidia/Video_Codec_SDK_11.1.5/Samples)

nx_add_test(nx_media_ut
    PUBLIC_LIBS
        nx_media
        nx_vms_common_test_support
    PROJECT VMS
    COMPONENT Client
    FOLDER client/tests
)

target_include_directories(nx_media_ut
    PRIVATE
        ${additional_sources_include_dirs}
)
