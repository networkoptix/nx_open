#pragma once

#include "options.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace nx {
namespace usb_cam {
namespace ffmpeg {

class Codec: public Options
{
public:
    ~Codec();

    int open();
    void close();

    int sendPacket(const AVPacket *packet) const;
    int sendFrame(const AVFrame * frame) const;

    int receivePacket(AVPacket * outPacket) const;
    int receiveFrame(AVFrame * outFrame) const;

    int initializeEncoder(AVCodecID codecId);
    int initializeEncoder(const char *codecName);

    int initializeDecoder(AVCodecID codecId);
    int initializeDecoder(const AVCodecParameters *codecParameters);
    int initializeDecoder(const char *codecName);

    void setFps(float fps);
    void setResolution(int width, int height);
    void setBitrate(int bitrate);
    void setPixelFormat(AVPixelFormat pixelFormat);

    void resolution(int * outWidth, int * outHeight) const;

    const AVCodecContext * codecContext() const;
    AVCodecContext * codecContext();
    const AVCodec * codec() const;
    AVCodecID codecId() const;

    // Video
    AVPixelFormat pixelFormat() const;

    // Audio
    AVSampleFormat sampleFormat() const;
    int sampleRate() const;
    int frameSize() const;

    void flush();

private:
    AVCodecContext *m_codecContext = nullptr;
    AVCodec *m_codec = nullptr;
};

} //namespace ffmpeg
} // namespace usb_cam
} // namespace nx
