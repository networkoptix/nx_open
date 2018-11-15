#pragma once

#include <nx/streaming/abstract_data_packet.h>

namespace nx::mediaserver::analytics {

/**
 * Wrapper for nx::sdk::analytics::DataPacket into internal VMS data packet.
 */
class DataPacketAdapter: public QnAbstractDataPacket
{
public:
    DataPacketAdapter(nx::sdk::analytics::DataPacket* packet): m_packet(packet) {}
    nx::sdk::analytics::DataPacket* packet() { return m_packet.get(); }
    const nx::sdk::analytics::DataPacket* packet() const { return m_packet.get(); }

private:
    nxpt::ScopedRef<nx::sdk::analytics::DataPacket> m_packet;
};

} // namespace nx::mediaserver::analytics
