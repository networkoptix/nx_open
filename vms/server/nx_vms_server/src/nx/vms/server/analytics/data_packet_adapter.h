#pragma once

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_data_packet.h>

#include <nx/streaming/abstract_data_packet.h>

namespace nx::vms::server::analytics {

/**
 * Wrapper for nx::sdk::analytics::IDataPacket into internal VMS data packet.
 */
class DataPacketAdapter: public QnAbstractDataPacket
{
public:
    DataPacketAdapter(sdk::Ptr<sdk::analytics::IDataPacket> packet):
        m_packet(std::move(packet))
    {
        this->timestamp = m_packet->timestampUs();
    }

    sdk::Ptr<sdk::analytics::IDataPacket> packet() { return m_packet; }
    sdk::Ptr<const sdk::analytics::IDataPacket> packet() const { return m_packet; }

private:
    sdk::Ptr<nx::sdk::analytics::IDataPacket> m_packet;
};

} // namespace nx::vms::server::analytics
