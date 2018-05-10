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
    AVCodecContainer(AVFormatContext * formatContext, AVStream * stream, bool isEncoder);
    AVCodecContainer(AVFormatContext * formatContext, AVCodecID codecID, bool isEncoder);
    ~AVCodecContainer();

    void open();
    void close();

    int encode(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket);
    int readAndDecode(AVFrame * outFrame, int* outGotPicture, AVPacket * packet, bool* outIsReadCode);

    bool isValid();

    AVCodecContext* getCodecContext();

    QString getAvError();

private:
    void initialize(AVCodecID codecID, bool isEncoder);

private:
    AVFormatContext * m_formatContext;
    AVCodecContext * m_codecContext;
    AVCodec * m_codec;

    AVStringError m_lastError;

    bool m_open;
};

