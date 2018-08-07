#include "abstract_stream_consumer.h"

namespace nx {
namespace ffmpeg {

AbstractPacketConsumer::AbstractPacketConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    m_streamReader(streamReader),
    m_params(params)
{
}

int AbstractPacketConsumer::fps() const
{
    return m_params.fps;
}

void AbstractPacketConsumer::resolution(int * width, int * height) const
{
    m_params.resolution(width, height);
}

int AbstractPacketConsumer::bitrate() const
{
    return m_params.bitrate;
}

void AbstractPacketConsumer::setFps(int fps)
{
    if(m_params.fps != fps)
    {
        m_params.fps = fps;
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateFps();
    }
}

void AbstractPacketConsumer::setResolution(int width, int height)
{
    if(m_params.width != width || m_params.height != height)
    {
        m_params.setResolution(width, height);
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateResolution();
    }
}

void AbstractPacketConsumer::setBitrate(int bitrate)
{
    if(m_params.bitrate != bitrate)
    {
        m_params.bitrate = bitrate;
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateBitrate();
    }
}


/////////////////////////////////////// AbstractFrameConsumer //////////////////////////////////////


AbstractFrameConsumer::AbstractFrameConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    m_streamReader(streamReader),
    m_params(params)
{
}

int AbstractFrameConsumer::fps() const
{
    return m_params.fps;
}

void AbstractFrameConsumer::resolution(int * width, int * height) const
{
    m_params.resolution(width, height);
}

int AbstractFrameConsumer::bitrate() const
{
    return m_params.bitrate;
}

void AbstractFrameConsumer::setFps(int fps)
{
    if(m_params.fps != fps)
    {
        m_params.fps = fps;
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateFps();
    }
}

void AbstractFrameConsumer::setResolution(int width, int height)
{
    if(m_params.width != width || m_params.height != height)
    {
        m_params.setResolution(width, height);
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateResolution();
    }
}

void AbstractFrameConsumer::setBitrate(int bitrate)
{
    if(m_params.bitrate != bitrate)
    {
        m_params.bitrate = bitrate;
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateBitrate();
    }
}
} // namespace ffmpeg
} // namespace nx