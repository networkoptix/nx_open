#pragma once

#include <stdint.h>
#include <atomic>
#include <memory>

extern"C"{
#include <libavutil/pixfmt.h>
#include <libavutil/frame.h>
}

namespace nx{
namespace usb_cam {
namespace ffmpeg {

class Frame
{
public:
    Frame();
    ~Frame();

    uint64_t timestamp() const;
    void setTimestamp(uint64_t millis);

    void unreference();

    const AVFrame * frame() const;
    AVFrame * frame();
    int64_t pts() const;
    int64_t packetPts() const;

    // Video
    AVPixelFormat pixelFormat() const;

    // Audio
    int nbSamples() const;
    AVSampleFormat sampleFormat() const;
    int sampleRate() const;
    uint64_t channelLayout() const;

    int getBuffer(AVPixelFormat format, int width, int height, int align = 32);
    int getBuffer(AVSampleFormat format, int nbSamples, uint64_t channelLayout, int align = 32);

private:
    uint64_t m_timestamp = 0;
    AVFrame * m_frame = nullptr;
};

} // namespace ffmpeg
} // namespace usb_cam
} // namespace nx
