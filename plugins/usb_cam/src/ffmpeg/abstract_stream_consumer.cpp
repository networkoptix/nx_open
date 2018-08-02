#include "abstract_stream_consumer.h"

namespace nx {
namespace ffmpeg {

AbstractStreamConsumer::AbstractStreamConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    m_streamReader(streamReader),
    m_params(params)
{
}

int AbstractStreamConsumer::fps() const
{
    return m_params.fps;
}

void AbstractStreamConsumer::resolution(int * width, int * height) const
{
    m_params.resolution(width, height);
}

int AbstractStreamConsumer::bitrate() const
{
    return m_params.bitrate;
}

void AbstractStreamConsumer::setFps(int fps)
{
    if(m_params.fps != fps)
    {
        m_params.fps = fps;
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateFps();
    }
}

void AbstractStreamConsumer::setResolution(int width, int height)
{
    if(m_params.width != width || m_params.height != height)
    {
        m_params.setResolution(width, height);
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateResolution();
    }
}

void AbstractStreamConsumer::setBitrate(int bitrate)
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