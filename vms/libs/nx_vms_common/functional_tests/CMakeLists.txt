## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(withTests AND LINUX)
    set(ft_base_path "${CMAKE_CURRENT_SOURCE_DIR}/rtsp_stream_provider_ft")

    nx_add_functional_test(
        rtsp_stream_provider_ft
        DEPENDS rtsp_checker
        FOLDER common/tests
        FILES
            ${ft_base_path}/Dockerfile
            ${ft_base_path}/Dockerfile.base
            ${ft_base_path}/gstreamer_rtsp_server.py
            ${ft_base_path}/rtsp_server.py
            ${ft_base_path}/entrypoint.sh
            ${CMAKE_SOURCE_DIR}/open/vms/libs/nx_vms_common/unit_tests/static-resources/test_b_frames.mkv
        BINARIES
            rtsp_checker
    )
endif()
