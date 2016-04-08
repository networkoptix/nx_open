#!/bin/bash
# To be run on nx1
clear
set -x
cd $(dirname "$0")

# Compile proxy_decoder.cpp -> libproxydecoder.so
# Link with /opt/ffmpeg
# Link with system libs: vdpau, cedrus, pixman-1
# Install to /opt/networkoptix/lite_client

if g++ -Wfatal-errors -std=c++11 -fPIC -shared -o libproxydecoder.so -L/opt/ffmpeg/lib -lavcodec -lavfilter -lavformat -lavutil -lavdevice -lswscale -lvdpau -lcedrus -lpixman-1 proxy_decoder.cpp; then
    cp -r libproxydecoder.so /opt/networkoptix/lite_client/lib/ && echo "SUCCESS"
else
    echo "FAILURE"
    exit 1
fi
