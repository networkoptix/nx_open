// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "old_api.h"

#include <nx/utils/log/log.h>

namespace nx::media::ffmpeg::old_api {

int encode(AVCodecContext* avctx, AVPacket* avpkt, const AVFrame* frame, int* got_packet)
{
    *got_packet = 0;
    int ret = avcodec_send_frame(avctx, frame);
    if (ret == AVERROR_EOF)
    {
        ret = 0;
    }
    else if (ret == AVERROR(EAGAIN))
    {
        // we fully drain all the output in each encode call, so this should not
        // ever happen
        return AVERROR_BUG;
    } else if (ret < 0)
    {
        return ret;
    }

    ret = avcodec_receive_packet(avctx, avpkt);
    if (ret < 0)
    {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;

        av_packet_unref(avpkt);
    }
    *got_packet = 1;
    return ret;
}

} // nx::media::ffmpeg::old_api
