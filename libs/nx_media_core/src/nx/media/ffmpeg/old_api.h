// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

extern "C" {

#include <libavcodec/avcodec.h>

} // extern "C"

namespace nx::media::ffmpeg::old_api {

// This implementation of encode/decode does not support the receiving multiple frames!
int NX_MEDIA_CORE_API decode(AVCodecContext* avctx, AVFrame* frame, int* got_frame, const AVPacket* pkt);
int NX_MEDIA_CORE_API encode(AVCodecContext* avctx, AVPacket* avpkt, const AVFrame* frame, int* got_packet);

} // nx::media::ffmpeg::old_api
