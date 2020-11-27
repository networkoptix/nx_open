#pragma once

#include <tuple>
#include <map>

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/sdk/uuid.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_metadata_packet.h>

#include <QtCore/QByteArray>

#include "events.h"

namespace nx::vms_server_plugins::analytics::dahua {

class DeviceAgent;

class MetadataParser: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    explicit MetadataParser(DeviceAgent* deviceAgent);

    virtual bool processData(const QnByteArrayConstRef& bytes) override;

    void terminateOngoingEvents();

private:
    struct OngoingEvent: Event
    {
        using Key = std::tuple<decltype(Event::type), decltype(Event::sequenceId)>;

        nx::sdk::Uuid trackId;
        nx::utils::ElapsedTimer sinceLastUpdate;
        nx::network::aio::Timer timer;
    };

private:
    static OngoingEvent::Key getKey(const Event& event);
    void parseEvent(const QByteArray& data);
    void process(const Event& event);
    void emitObject(const Event& event, const nx::sdk::Uuid& trackId);
    void emitEvent(const Event& event, const nx::sdk::Uuid& trackId);
    void emitMetadataPacket(const nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket>& packet);
    void terminate(const OngoingEvent& ongoingEvent);

private:
    DeviceAgent* const m_deviceAgent;
    nx::network::aio::BasicPollable m_basicPollable;
    std::map<OngoingEvent::Key, OngoingEvent> m_ongoingEvents;
};

} // namespace nx::vms_server_plugins::analytics::dahua
