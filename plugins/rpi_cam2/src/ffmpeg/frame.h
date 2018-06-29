#pragma once

extern"C"{
#include<libavutil/pixfmt.h>
}

class AVFrame;

namespace nx{
namespace ffmpeg {

class Frame
{
public:
    Frame();
    ~Frame();

    void unreference();

    AVFrame * frame() const;

    int imageAlloc(int width, int height, AVPixelFormat format, int align);
    void freeData();

private:
    AVFrame * m_frame = nullptr;
    bool m_imageAllocated = false;
};

} // namespace ffmpeg
} // namespace nx