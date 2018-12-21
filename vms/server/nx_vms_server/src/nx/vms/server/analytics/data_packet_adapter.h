#pragma once

#include <nx/sdk/common/ptr.h>
#include <nx/streaming/abstract_data_packet.h>

namespace nx::vms::server::analytics {

/**
 * Wrapper for nx::sdk::analytics::IDataPacket into internal VMS data packet.
 */
class DataPacketAdapter: public QnAbstractDataPacket
{
public:
    DataPacketAdapter(nx::sdk::analytics::IDataPacket* packet): m_packet(
        [packet]() { packet->addRef(); return packet; }()) {}
    nx::sdk::analytics::IDataPacket* packet() { return m_packet.get(); }
    const nx::sdk::analytics::IDataPacket* packet() const { return m_packet.get(); }

private:
    nx::sdk::common::Ptr<nx::sdk::analytics::IDataPacket> m_packet;
};

} // namespace nx::vms::server::analytics
