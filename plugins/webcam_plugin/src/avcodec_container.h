#pragma once

#include <QtCore/Qstring>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

//#include "libav_forward_declarations.h"
#include "av_string_error.h"

class AVCodecContainer
{
public:
    AVCodecContainer(AVFormatContext * formatContext);
    ~AVCodecContainer();

    void open();
    void close();

    int encodeVideo(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket);
    int decodeVideo(AVFrame * outFrame, int* outGotPicture, AVPacket * packet);
    
    void initializeEncoder(AVCodecID codecID);
    void initializeDecoder(AVCodecParameters * codecParameters);
    bool isValid();

    AVCodecContext* getCodecContext();
    QString getAvError();

private:
    AVFormatContext * m_formatContext;
    AVCodecContext * m_codecContext;
    AVCodec * m_codec;

    AVStringError m_lastError;

    bool m_open;
};

