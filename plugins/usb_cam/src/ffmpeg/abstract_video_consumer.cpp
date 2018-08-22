#include "abstract_video_consumer.h"

namespace nx {
namespace ffmpeg {

AbstractVideoConsumer::AbstractVideoConsumer(
    const std::weak_ptr<VideoStreamReader>& streamReader,
    const CodecParameters& params)
    :
    m_streamReader(streamReader),
    m_params(params)
{
}

int AbstractVideoConsumer::fps() const
{
    return m_params.fps;
}

void AbstractVideoConsumer::resolution(int * width, int * height) const
{
    m_params.resolution(width, height);
}

int AbstractVideoConsumer::bitrate() const
{
    return m_params.bitrate;
}

void AbstractVideoConsumer::setFps(int fps)
{
    if(m_params.fps != fps)
    {
        m_params.fps = fps;
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateFps();
    }
}

void AbstractVideoConsumer::setResolution(int width, int height)
{
    if(m_params.width != width || m_params.height != height)
    {
        m_params.setResolution(width, height);
        if (auto streamReader = m_streamReader.lock())
            streamReader->updateResolution();
    }
}

void AbstractVideoConsumer::setBitrate(int bitrate)
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