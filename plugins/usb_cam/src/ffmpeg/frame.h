#pragma once

#include <stdint.h>
#include <atomic>
#include <memory>

extern"C"{
#include <libavutil/pixfmt.h>
#include <libavutil/frame.h>
}

namespace nx{
namespace ffmpeg {

class Frame
{
public:
    Frame(const std::shared_ptr<std::atomic_int>& frameCount = nullptr);
    ~Frame();

    uint64_t timeStamp() const;
    void setTimeStamp(uint64_t millis);

    void unreference();

    AVFrame * frame() const;
    int64_t pts() const;
    int64_t packetPts() const;
    int nbSamples() const;

    int getBuffer(AVPixelFormat format, int width, int height, int align = 32);
    int getBuffer(AVSampleFormat format, int nbSamples, uint64_t channelLayout, int align = 32);

private:
    uint64_t m_timeStamp;
    AVFrame * m_frame = nullptr;
    std::shared_ptr<std::atomic_int> m_frameCount;
};

} // namespace ffmpeg
} // namespace nx