#include "frame.h"

extern "C"{
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
} // extern "C"

namespace nx {
namespace ffmpeg {

Frame::Frame(const std::shared_ptr<std::atomic_int>& frameCount):
    m_frame(av_frame_alloc()),
    m_frameCount(frameCount)
{
    if (m_frameCount)
        ++(*m_frameCount);
}

Frame::~Frame()
{
    if (m_frame)
    {
        freeData();
        av_frame_free(&m_frame);
    }
    
    if (m_frameCount)
        --(*m_frameCount);
}

uint64_t Frame::timeStamp() const
{
    return m_timeStamp;
}

void Frame::setTimeStamp(uint64_t millis)
{
    m_timeStamp = millis;
}

void Frame::unreference()
{
    av_frame_unref(m_frame);
}

AVFrame * Frame::frame() const
{
    return m_frame;
}

int64_t Frame::pts() const
{
    return m_frame->pts;
}

int Frame::allocateImage(int width, int height, AVPixelFormat format, int align)
{
    int allocCode = av_image_alloc(m_frame->data, m_frame->linesize, width, height, format, align);
    if (allocCode < 0)
        return allocCode;
        
    m_frame->width = width;
    m_frame->height = height;
    m_frame->format = format;
    
    m_imageAllocated = true;
    return allocCode;
}

void Frame::freeData()
{
    if (m_imageAllocated)
    {
        av_freep(&m_frame->data[0]);
        m_imageAllocated = false;
    }
}

} // namespace ffmpeg
} // namespace nx