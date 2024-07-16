// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "old_api.h"

#include <nx/utils/log/log.h>

namespace nx::media::ffmpeg::old_api {

// Simplify the old api implementation by not checking when multiple frames can be received in a
// sent packet.
int decode(AVCodecContext* avctx, AVFrame* frame, int* got_frame, const AVPacket* pkt)
{
    *got_frame = 0;
    int result = 0;
    result = avcodec_send_packet(avctx, pkt);
    if (result == AVERROR_EOF)
        result = 0;
    else if (result == AVERROR(EAGAIN)) {
        /* we fully drain all the output in each decode call, so this should not
            * ever happen */
        result = AVERROR_BUG;
        goto finish;
    } else if (result < 0)
        goto finish;

    result = avcodec_receive_frame(avctx, frame);
    if (result < 0) {
        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            result = 0;
        goto finish;
    }
    *got_frame = 1;

finish:
    if (result == 0 && pkt)
        result = pkt->size;

    return result;
}

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
