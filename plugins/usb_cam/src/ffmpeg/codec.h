#pragma once

#include "options.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace nx {
namespace ffmpeg {

class Codec : public Options
{
public:
    Codec();
    ~Codec();

    int open();
    void close();

    int sendPacket(const AVPacket *packet) const;
    int sendFrame(const AVFrame * frame) const;

    int receivePacket(AVPacket * outPacket) const;
    int receiveFrame(AVFrame * outFrame) const;

    int encode(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket) const;
    int decode(AVFrame *outFrame, int *outGotFrame, const AVPacket *packet) const;

    int encodeVideo(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket) const;
    int decodeVideo(AVFrame *outFrame, int *outGotPicture, const AVPacket *packet) const;

    int encodeAudio(AVPacket *outPacket, const AVFrame *frame, int *outGotPacket) const;
    int decodeAudio(AVFrame *frame, int *outGotFrame, const AVPacket *packet) const;

    int initializeEncoder(AVCodecID codecID);
    int initializeEncoder(const char *codecName);

    int initializeDecoder(AVCodecParameters *codecParameters);
    int initializeDecoder(AVCodecID codecID);
    int initializeDecoder(const char *codecName);

    void setFps(float fps);
    void setResolution(int width, int height);
    void setBitrate(int bitrate);
    void setPixelFormat(AVPixelFormat pixelFormat);

    void resolution(int * outWidth, int * outHeight) const;

    AVCodecContext * codecContext() const;
    AVCodec * codec() const;
    AVCodecID codecID() const;
    AVPixelFormat pixelFormat() const;

private:
    AVCodecContext *m_codecContext = nullptr;
    AVCodec *m_codec = nullptr;
};

} //namespace ffmpeg
} // namespace nx
