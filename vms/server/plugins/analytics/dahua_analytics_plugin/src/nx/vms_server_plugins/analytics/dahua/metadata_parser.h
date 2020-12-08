#pragma once

#include <tuple>
#include <map>
#include <optional>

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/sdk/uuid.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/i_utility_provider.h>

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
    class ObjectIdToTrackIdMap
    {
    public:
        nx::sdk::Uuid operator()(const decltype(Object::id)& objectId);

    private:
        struct Entry
        {
            nx::sdk::Uuid trackId = nx::sdk::UuidHelper::randomUuid();
            nx::utils::ElapsedTimer sinceLastAccess;
        };

    private:
        void cleanUpStaleEntries();

    private:
        std::map<decltype(Object::id), Entry> m_entries;
        nx::utils::ElapsedTimer m_sinceLastCleanup{/*started*/ true};
    };

    struct OngoingEvent: Event
    {
        using Key = std::tuple<decltype(Event::type), decltype(Event::sequenceId)>;

        std::optional<Object> primaryObject;
        nx::utils::ElapsedTimer sinceLastUpdate;
        nx::network::aio::Timer timer;
    };

private:
    static OngoingEvent::Key getKey(const Event& event);
    void parseEvent(const QByteArray& data);
    void process(const Event& event);
    void emitObjects(const Event& event);
    void emitEvent(const Event& event, const Object* primaryObject = nullptr);
    void emitMetadataPacket(const nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket>& packet);
    void terminate(const OngoingEvent& ongoingEvent);

private:
    DeviceAgent* const m_deviceAgent;
    const nx::sdk::Ptr<nx::sdk::IUtilityProvider> m_utilityProvider;
    nx::network::aio::BasicPollable m_basicPollable;
    ObjectIdToTrackIdMap m_objectIdToTrackIdMap;
    std::map<OngoingEvent::Key, OngoingEvent> m_ongoingEvents;
};

} // namespace nx::vms_server_plugins::analytics::dahua
