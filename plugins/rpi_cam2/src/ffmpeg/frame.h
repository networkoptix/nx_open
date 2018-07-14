#pragma once

#include <stdint.h>

extern"C"{
#include<libavutil/pixfmt.h>
}

struct AVFrame;

namespace nx{
namespace ffmpeg {

class Frame
{
public:
    Frame();
    ~Frame();

    uint64_t timeStamp() const;
    void setTimeStamp(uint64_t millis);

    void unreference();

    AVFrame * frame() const;

    int allocateImage(int width, int height, AVPixelFormat format, int align);
    void freeData();

private:
    uint64_t m_timeStamp;
    AVFrame * m_frame = nullptr;
    bool m_imageAllocated = false;
};

} // namespace ffmpeg
} // namespace nx