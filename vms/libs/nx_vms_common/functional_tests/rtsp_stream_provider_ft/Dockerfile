## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

ARG BASE_IMAGE=rtsp_stream_provider_ft_base:latest
FROM ${BASE_IMAGE}

WORKDIR /testdir

ENV LD_LIBRARY_PATH=/testdir/lib

COPY rtsp_stream_provider_ft/rtsp_checker ./rtsp_checker
COPY rtsp_stream_provider_ft/lib ./lib
COPY rtsp_stream_provider_ft/gstreamer_rtsp_server.py ./gstreamer_rtsp_server.py
COPY rtsp_stream_provider_ft/rtsp_server.py ./rtsp_server.py
COPY rtsp_stream_provider_ft/test_b_frames.mkv ./test_b_frames.mkv
COPY rtsp_stream_provider_ft/entrypoint.sh ./entrypoint.sh

RUN test -f ./rtsp_checker \
    && test -x ./rtsp_checker \
    && ! ldd ./rtsp_checker | grep "not found" \
    && python3 -c 'import gi; gi.require_version("GstRtspServer", "1.0")' \
    && gst-inspect-1.0 srtpenc | grep --quiet "aes-128-gcm"

ENTRYPOINT ["/testdir/entrypoint.sh"]
CMD []
