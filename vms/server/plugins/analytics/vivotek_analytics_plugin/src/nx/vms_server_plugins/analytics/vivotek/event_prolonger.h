#pragma once

#include <cstdint>
#include <optional>
#include <tuple>
#include <map>
#include <deque>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_event_metadata.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/aio/timer.h>

#include <QtCore/QJsonValue>
#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::vivotek {

class EventProlonger
{
public:
    void write(nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket> packet);
    void flush();

    cf::future<nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>> read();
    nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket> readSync();

private:
    struct OngoingEvent
    {
        nx::network::aio::Timer timer;
        nx::sdk::Ptr<const nx::sdk::analytics::IEventMetadata> metadata;
        std::int64_t timestampDeltaUs;
    };

private:
    void emitEvent(const nx::sdk::Ptr<const nx::sdk::analytics::IEventMetadata>& metadata,
        bool isActive, std::int64_t timestampUs);

    void pump();

private:
    std::deque<nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>> m_packets;
    std::optional<cf::promise<nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>>> m_promise;
    std::map<std::tuple<QString, QString>, OngoingEvent> m_ongoingEvents;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
