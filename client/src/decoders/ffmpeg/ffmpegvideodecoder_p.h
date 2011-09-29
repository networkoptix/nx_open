#ifndef FFMPEGVIDEODECODER_P_H
#define FFMPEGVIDEODECODER_P_H

#include "decoders/abstractvideodecoder.h"

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
#ifdef _USE_DXVA
class DxvaDecoderContext;
#endif

// client of this class is responsible for encoded data buffer meet ffmpeg restrictions
// (see comment to decode functions for details).

class CLFFmpegVideoDecoder : public CLAbstractVideoDecoder
{
public:
    CLFFmpegVideoDecoder(int codecId, AVCodecContext *codecContext = 0);
    ~CLFFmpegVideoDecoder();

    static bool isCodecSupported(int codecId);
    static bool isHardwareAccellerationPossible(int codecId, int width, int height);

    bool decode(CLVideoData &data);
    void showMotion(bool show);

private:
    void resetDecoder();
    void createDecoder();
    void destroyDecoder();

private:
    AVCodec *m_codec;
    AVCodecContext *m_context;
    AVCodecContext *m_passedContext;
    AVFrame *m_frame;

    uchar *m_deinterlaceBuffer;
    AVFrame *m_deinterlacedFrame;

#ifdef _USE_DXVA
    DxvaDecoderContext *m_dxvaDecoderContext;
#endif

    int m_width;
    int m_height;

    int m_codecId;
    bool m_showmotion;

    int m_lastWidth;
};

#endif // FFMPEGVIDEODECODER_P_H
