#pragma once

#include <stdint.h>
#include <atomic>
#include <memory>

extern"C"{
#include<libavutil/pixfmt.h>
}

struct AVFrame;

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

    int allocateImage(int width, int height, AVPixelFormat format, int align);
    void freeData();

private:
    uint64_t m_timeStamp;
    AVFrame * m_frame = nullptr;
    bool m_imageAllocated = false;
    std::shared_ptr<std::atomic_int> m_frameCount;
};

} // namespace ffmpeg
} // namespace nx