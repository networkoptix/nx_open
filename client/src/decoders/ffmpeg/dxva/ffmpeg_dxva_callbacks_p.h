#ifndef FFMPEG_DXVA_CALLBACKS_H
#define FFMPEG_DXVA_CALLBACKS_H

#include "dxva_p.h"

/**
* FFmpeg DXVA callbacks and utilities
*/
struct FFmpegDXVACallbacks
{
    static int ffmpeg_GetFrameBuf(struct AVCodecContext *p_context, AVFrame *p_ff_pic);
    static void ffmpeg_ReleaseFrameBuf(struct AVCodecContext *p_context, AVFrame *p_ff_pic);
    static PixelFormat ffmpeg_GetFormat(AVCodecContext *p_codec, const enum PixelFormat *pi_fmt);
};

#endif // FFMPEG_DXVA_CALLBACKS_H
