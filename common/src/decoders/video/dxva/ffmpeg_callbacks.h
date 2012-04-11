#ifndef universal_client_ffmpeg_dxva_h_
#define universal_client_ffmpeg_dxva_h_

/**
* Ffmpeg callbacks and utilities
*/
struct FFMpegCallbacks
{
    // Callbacks
    static int ffmpeg_GetFrameBuf(struct AVCodecContext *p_context, AVFrame *p_ff_pic);
    static void ffmpeg_ReleaseFrameBuf(struct AVCodecContext *p_context, AVFrame *p_ff_pic);
    static PixelFormat ffmpeg_GetFormat(AVCodecContext *p_codec, const enum PixelFormat *pi_fmt);
};

#endif // universal_client_ffmpeg_dxva_h_
