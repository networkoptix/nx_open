#!/bin/bash
# To be run on nx1
clear
set -x
cd $(dirname "$0")

# Compile libproxy_decoder.so
# Link with /opt/ffmpeg
# Link with system libs: vdpau, cedrus, pixman-1
# Install to /opt/networkoptix/lite_client

if \
g++ -Wfatal-errors -std=c++11 -fPIC -shared -o libproxydecoder.so \
    -L/opt/ffmpeg/lib -lavcodec -lavfilter -lavformat -lavutil -lavdevice -lswscale \
    -lvdpau -lcedrus -lpixman-1 \
    conf.cpp vdpau_helper.cpp proxy_decoder_*.cpp \
; then
    cp -r libproxydecoder.so /opt/networkoptix/lite_client/lib/ || exit 1
    cp -r libproxydecoder.so ~/develop/buildenv/packages/bpi/proxy-decoder/lib/ || exit 1
    cp -r proxy_decoder.h ~/develop/buildenv/packages/bpi/proxy-decoder/include/ || exit 1
    echo "SUCCESS"
else
    echo "FAILURE"
    exit 1
fi
