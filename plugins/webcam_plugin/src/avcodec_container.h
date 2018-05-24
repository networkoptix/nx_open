#pragma once

#include <QtCore/QString>
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
    ~AVCodecContainer();

    int open();
    int close();

    static int readFrame(AVFormatContext * formatContext, AVPacket * outPacket);

    int encodeVideo(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket) const;
    int decodeVideo(AVFormatContext* formatContext, AVFrame *outFrame, int *outGotPicture, AVPacket *packet) const;

    int encodeAudio(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket) const;
    int decodeAudio(AVFrame * frame, int* outGotFrame, const AVPacket *packet) const;

    void initializeEncoder(AVCodecID codecID);
    void initializeDecoder(AVCodecParameters * codecParameters);
    void initializeDecoder(AVCodecID codecID);
    bool isValid() const;

    AVStringError lastError() const;

    AVCodecContext* codecContext() const;
    AVCodec* codec() const;
    AVCodecID codecID() const;

    QString avErrorString() const;

private:
    AVCodecContext * m_codecContext = nullptr;
    AVCodec * m_codec = nullptr;
    bool m_open = false;

    AVStringError m_lastError;
};

} // namespace webcam_plugin
} // namespace nx
