#pragma once

#include <optional>
#include <deque>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/aio/timer.h>

#include <QtCore/QJsonValue>

namespace nx::vms_server_plugins::analytics::vivotek {

class EventProlonger
{
public:
    void write(nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket> packet);
    cf::future<nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>> read();

private:
    void pump();

private:
    std::deque<nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>> m_packets;
    std::optional<cf::promise<nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>>> m_promise;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
