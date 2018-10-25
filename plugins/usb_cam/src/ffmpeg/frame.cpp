#include "frame.h"

extern "C"{
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
} // extern "C"

namespace nx {
namespace usb_cam {
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
        av_frame_free(&m_frame);
    
    if (m_frameCount)
        --(*m_frameCount);
}

uint64_t Frame::timestamp() const
{
    return m_timestamp;
}

void Frame::setTimestamp(uint64_t millis)
{
    m_timestamp = millis;
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

int64_t Frame::packetPts() const
{
    return m_frame->pkt_pts;
}

AVPixelFormat Frame::pixelFormat() const
{
    return (AVPixelFormat)m_frame->format;
}

int Frame::nbSamples() const
{
    return m_frame->nb_samples;
}

AVSampleFormat Frame::sampleFormat() const
{
    return (AVSampleFormat)m_frame->format;
}

int Frame::sampleRate() const
{
    return m_frame->sample_rate;
}

uint64_t Frame::channelLayout() const
{
    return m_frame->channel_layout;
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
} // namespace usb_cam
} // namespace nx