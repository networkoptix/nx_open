#pragma once

#include <unordered_set>
#include <map>
#include <tuple>
#include <optional>

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>
#include <nx/sdk/uuid.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/i_utility_provider.h>
#include <nx/sdk/analytics/i_metadata_types.h>
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
    void setNeededTypes(const nx::sdk::Ptr<const nx::sdk::analytics::IMetadataTypes>& types);
    void terminateOngoingEvents();

private:
    struct OngoingEvent: Event
    {
        std::optional<Object> singleCausingObject;
        nx::sdk::Uuid trackId;
    };

private:
    void parseEvent(const QByteArray& data);
    void process(const Event& event);

    void emitObject(const Object& object, const nx::sdk::Uuid& trackId, const Event& event);

    void emitEvent(
        const Event& event, const nx::sdk::Uuid* trackId, const Object* singleCausingObject);

    void emitMetadataPacket(const nx::sdk::Ptr<nx::sdk::analytics::IMetadataPacket>& packet);

private:
    DeviceAgent* const m_deviceAgent;
    std::unordered_set<const EventType*> m_neededEventTypes;
    std::map<decltype(Event::type), OngoingEvent> m_ongoingEvents;
};

} // namespace nx::vms_server_plugins::analytics::dahua
