#!/bin/sh
BUILDENV="../../../buildenv"
cd $(dirname "$0")

# Build @bpi:
# make clean build

cp -r libproxydecoder.so "$BUILDENV/packages/bpi/proxy-decoder/lib/" || exit 1
cp -r proxy_decoder.h "$BUILDENV/packages/bpi/proxy-decoder/include/" || exit 1

cd "$BUILDENV/packages/bpi/proxy-decoder" && rdep -u
