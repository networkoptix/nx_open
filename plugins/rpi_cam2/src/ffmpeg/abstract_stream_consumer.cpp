#include "abstract_stream_consumer.h"

namespace nx {
namespace ffmpeg {

AbstractStreamConsumer::AbstractStreamConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    m_streamReader(streamReader),
    m_params(m_params)
{
}

AbstractStreamConsumer::~AbstractStreamConsumer()
{
    if(auto sr = m_streamReader.lock())
        sr->removeConsumer(weak_from_this());
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

void AbstractStreamConsumer::initialize()
{
    if(m_ready)
        return;

    m_ready = true;
    m_streamReader.lock()->addConsumer(weak_from_this());
}

void AbstractStreamConsumer::setFps(int fps)
{
    if(m_params.fps != fps)
    {
        m_params.fps = fps;
        m_streamReader.lock()->updateFps();
    }
}

void AbstractStreamConsumer::setResolution(int width, int height)
{
    if(m_params.width != width || m_params.height != height)
    {
        m_params.setResolution(width, height);
        m_streamReader.lock()->updateResolution();
    }
}

void AbstractStreamConsumer::setBitrate(int bitrate)
{
    if(m_params.bitrate != bitrate)
    {
        m_params.bitrate = bitrate;
        m_streamReader.lock()->updateBitrate();
    }
}

}
}