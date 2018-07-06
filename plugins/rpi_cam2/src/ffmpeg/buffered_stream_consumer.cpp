#include "buffered_stream_consumer.h"

namespace nx {
namespace ffmpeg {

BufferedStreamConsumer::BufferedStreamConsumer(
    const CodecParameters& params, 
    const std::weak_ptr<StreamReader>& streamReader)
    :
    StreamConsumer(streamReader),
    m_params(params),
    m_queueSize(60)
{   
}

void BufferedStreamConsumer::resize(int size)
{
    m_queueSize = size;
}

void BufferedStreamConsumer::setFps(int fps)
{
    m_params.fps = fps;
    if (auto sr = m_streamReader.lock())
        sr->setFps(fps);
}

void BufferedStreamConsumer::setResolution(int width, int height)
{
    m_params.setResolution(width, height);
    if (auto sr = m_streamReader.lock())
        sr->setResolution(width, height);
}

void BufferedStreamConsumer::setBitrate(int bitrate)
{
    m_params.bitrate = bitrate;
    if (auto sr = m_streamReader.lock())
        sr->setBitrate(bitrate);
}

int BufferedStreamConsumer::fps() const
{
    return m_params.fps;
}

void BufferedStreamConsumer::resolution(int * width, int * height) const
{   
    m_params.resolution(width, height);
}

int BufferedStreamConsumer::bitrate() const
{
    return m_params.bitrate;
}

void BufferedStreamConsumer::givePacket(const std::shared_ptr<Packet>& packet)
{
    m_packets.push(packet);
}

std::shared_ptr<Packet> BufferedStreamConsumer::popNextPacket()
{
    return m_packets.isEmpty() ? nullptr : m_packets.pop();
}

}
}