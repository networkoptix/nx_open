#pragma once

#include <QtCore/Qstring>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include "av_string_error.h"

namespace nx {
namespace webcam_plugin {

class AVCodecContainer
{
public:
    AVCodecContainer(AVFormatContext * formatContext);
    ~AVCodecContainer();

    void open();
    void close();

    int readFrame(AVPacket * outPacket);

    int encodeVideo(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket);
    int decodeVideo(AVFrame *outFrame, int *outGotPicture, AVPacket *packet);

    int encodeAudio(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket);
    int decodeAudio(AVFrame * frame, int* outGotFrame, const AVPacket *packet);
    
    void initializeEncoder(AVCodecID codecID);
    void initializeDecoder(AVCodecParameters * codecParameters);
    void initializeDecoder(AVCodecID codecID);
    bool isValid();


    AVCodecContext* codecContext();
    AVCodecID codecID();

    QString avErrorString();

private:
    AVFormatContext * m_formatContext;
    AVCodecContext * m_codecContext;
    AVCodec * m_codec;

    AVStringError m_lastError;

    bool m_open;
};

} // namespace webcam_plugin
} // namespace nx
