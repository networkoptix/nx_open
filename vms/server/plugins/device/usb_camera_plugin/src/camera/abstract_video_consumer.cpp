#include "abstract_video_consumer.h"

#include "video_stream.h"

namespace nx {
namespace usb_cam {

AbstractVideoConsumer::AbstractVideoConsumer(
    const std::weak_ptr<VideoStream>& videoStream,
    const CodecParameters& params)
    :
    m_videoStream(videoStream),
    m_params(params)
{
}

float AbstractVideoConsumer::fps() const
{
    return m_params.fps;
}

nxcip::Resolution AbstractVideoConsumer::resolution() const
{
    return m_params.resolution;
}

int AbstractVideoConsumer::bitrate() const
{
    return m_params.bitrate;
}

void AbstractVideoConsumer::setFps(float fps)
{
    if(m_params.fps != fps)
    {
        m_params.fps = fps;
        if (auto streamReader = m_videoStream.lock())
            streamReader->updateFps();
    }
}

void AbstractVideoConsumer::setResolution(const nxcip::Resolution& resolution)
{
    if(m_params.resolution.width != resolution.width 
        || m_params.resolution.height != resolution.height)
    {
        m_params.resolution = resolution;
        if (auto streamReader = m_videoStream.lock())
            streamReader->updateResolution();
    }
}

void AbstractVideoConsumer::setBitrate(int bitrate)
{
    if(m_params.bitrate != bitrate)
    {
        m_params.bitrate = bitrate;
        if (auto streamReader = m_videoStream.lock())
            streamReader->updateBitrate();
    }
}

} // namespace usb_cam
} // namespace nx