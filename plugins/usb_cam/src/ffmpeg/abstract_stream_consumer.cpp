#include "abstract_stream_consumer.h"

#include "../utils/utils.h"

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
        debug("AbstractStreamConsumer::setFPS(): %d\n", fps);
        m_params.fps = fps;
        m_streamReader.lock()->updateFps();
    }
}

void AbstractStreamConsumer::setResolution(int width, int height)
{
    if(m_params.width != width || m_params.height != height)
    {
        debug("AbstractStreamConsumer::setResolution(): %d, %d\n", width, height);
        m_params.setResolution(width, height);
        m_streamReader.lock()->updateResolution();
    }
}

void AbstractStreamConsumer::setBitrate(int bitrate)
{
    if(m_params.bitrate != bitrate)
    {
        debug("AbstractStreamConsumer::setBitrate(): %d\n", bitrate);
        m_params.bitrate = bitrate;
        m_streamReader.lock()->updateBitrate();
    }
}

}
}