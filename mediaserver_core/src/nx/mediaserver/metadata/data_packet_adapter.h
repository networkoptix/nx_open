#pragma once

#include <nx/streaming/abstract_data_packet.h>

namespace nx {
namespace mediaserver {
namespace metadata {

/**
 * Wrapper for nx::sdk::metadata::DataPacket into internal VMS data packet.
 */
class DataPacketAdapter: public QnAbstractDataPacket
{
public:
    DataPacketAdapter(nx::sdk::metadata::DataPacket* packet): m_packet(packet) {}
    nx::sdk::metadata::DataPacket* packet() { return m_packet.get(); }
    const nx::sdk::metadata::DataPacket* packet() const { return m_packet.get(); }

private:
    nxpt::ScopedRef<nx::sdk::metadata::DataPacket> m_packet;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
