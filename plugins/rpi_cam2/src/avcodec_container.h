#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

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

    int initializeEncoder(AVCodecID codecID);
    int initializeEncoder(const char * codecName);
    int initializeDecoder(AVCodecParameters * codecParameters);
    int initializeDecoder(const char * codecName);

    int addOption(const char * key, const char * value, int flags = 0);

    AVCodecContext* codecContext() const;
    AVCodec* codec() const;
    AVDictionary * options() const;
    AVCodecID codecID() const;

private:
    AVCodecContext * m_codecContext = nullptr;
    AVCodec * m_codec = nullptr;
    AVDictionary * m_options = nullptr;
    bool m_open = false;
};

} // namespace webcam_plugin
} // namespace nx
