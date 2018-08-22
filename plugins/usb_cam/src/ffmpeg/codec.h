#pragma once

#include "options.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace nx {
namespace ffmpeg {

class Codec : public Options
{
public:
    ~Codec();

    int open();
    void close();

    int sendPacket(const AVPacket *packet) const;
    int sendFrame(const AVFrame * frame) const;

    int receivePacket(AVPacket * outPacket) const;
    int receiveFrame(AVFrame * outFrame) const;

    int decode(AVFrame * outFrame, const AVPacket * packet, int * outGotFrame);
    int encode(const AVFrame * frame, AVPacket * outPacket, int * outGotPacket);

    int decodeVideo(AVFrame * outFrame, const AVPacket * packet, int * outGotFrame);
    int encodeVideo(const AVFrame * frame, AVPacket * outPacket, int * outGotPacket);

    int decodeAudio(AVFrame * outFrame, const AVPacket * packet, int * outGotFrame);
    int encodeAudio(const AVFrame * frame, AVPacket * outPacket, int * outGotPacket);

    int initializeEncoder(AVCodecID codecID);
    int initializeEncoder(const char *codecName);

    int initializeDecoder(AVCodecID codecID);
    int initializeDecoder(const AVCodecParameters *codecParameters);
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

    void flush() const;

private:
    AVCodecContext *m_codecContext = nullptr;
    AVCodec *m_codec = nullptr;
};

} //namespace ffmpeg
} // namespace nx
