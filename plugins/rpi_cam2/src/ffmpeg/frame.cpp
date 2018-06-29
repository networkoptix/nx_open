#include "frame.h"

extern "C"{
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
} // extern "C"

#include "error.h"

namespace nx {
namespace ffmpeg {

Frame::Frame():
    m_frame(av_frame_alloc())
{
    if (!m_frame)
        error::setLastError(AVERROR(ENOMEM));
}

Frame::~Frame()
{
    if (m_frame)
    {
        freeData();
        av_frame_free(&m_frame);
    }
}

void Frame::unreference()
{
    av_frame_unref(m_frame);
}

AVFrame * Frame::frame() const
{
    return m_frame;
}

int Frame::imageAlloc(int width, int height, AVPixelFormat format, int align)
{
    int allocCode = av_image_alloc(m_frame->data, m_frame->linesize, width, height, format, align);
    if (error::updateIfError(allocCode))
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