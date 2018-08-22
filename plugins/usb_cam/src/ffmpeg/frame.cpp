#include "frame.h"

extern "C"{
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
} // extern "C"

namespace nx {
namespace ffmpeg {

Frame::Frame(const std::shared_ptr<std::atomic_int>& frameCount):
    m_frame(av_frame_alloc()),
    m_frameCount(frameCount),
    m_allocated(false)
{
    if (m_frameCount)
        ++(*m_frameCount);
}

Frame::~Frame()
{
    if (m_frame)
    {
        //freeData();
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
    // int result = av_image_alloc(m_frame->data, m_frame->linesize, width, height, format, align);
    // if (result < 0)
    //     return result;

    m_frame->width = width;
    m_frame->height = height;
    m_frame->format = format;

    int result = av_frame_get_buffer(m_frame, align);
    
    m_allocated = true;
    return result;
}

int Frame::allocateAudio(int nbChannels, int nbSamples, AVSampleFormat format, int align)
{
    // int result = 
    //     av_samples_alloc(m_frame->data, m_frame->linesize, nbChannels, nbSamples, format, align);
    // if (result < 0)
    //     return result;

    av_frame_set_channels(m_frame, nbChannels);
    m_frame->nb_samples = nbSamples;
    m_frame->format = format;
    
    int result = av_frame_get_buffer(m_frame, align);
    if(result < 0)
        return result;

    m_allocated = true;
    //return result;
    return 0;
}

void Frame::freeData()
{
    if (m_allocated)
    {
        av_freep(&m_frame->data[0]);
        m_allocated = false;
    }
}

int Frame::getBuffer(AVPixelFormat format, int width, int height, int align)
{
    m_frame->format = format;
    m_frame->width = width;
    m_frame->height = height;
    return av_frame_get_buffer(m_frame, align);
}

int Frame::getBuffer(AVSampleFormat format, int nbSamples, uint64_t channelLayout, int align)
{
    m_frame->format = format;
    m_frame->nb_samples = nbSamples;
    m_frame->channel_layout = channelLayout;
    return av_frame_get_buffer(m_frame, align);
}

} // namespace ffmpeg
} // namespace nx