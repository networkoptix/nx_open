#include "buffered_stream_consumer.h"

#include "packet.h"

namespace nx {
namespace ffmpeg {

BufferedStreamConsumer::BufferedStreamConsumer(
    const std::weak_ptr<StreamReader>& streamReader,
    const CodecParameters& params)
    :
    AbstractStreamConsumer(streamReader, params)
{   
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