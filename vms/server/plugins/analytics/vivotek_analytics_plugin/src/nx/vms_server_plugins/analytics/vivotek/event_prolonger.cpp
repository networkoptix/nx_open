#include "event_prolonger.h"

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>

#include <QtCore/QDateTime>

#include "event_types.h"
#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::utils;

namespace {

const auto kTimeout = 10s;

std::int64_t nowUs()
{
    return QDateTime::currentMSecsSinceEpoch() * 1000;
}

bool isProlongedEvent(const QString& typeId)
{
    for (const auto& type: kEventTypes)
    {
        if (type.id == typeId)
            return type.isProlonged;
    }

    throw Exception("Unknown even type id: %1", typeId);
}

} // namespace

void EventProlonger::write(Ptr<IEventMetadataPacket> packet)
{
    for (int i = 0; i < packet->count(); ++i)
    {
        auto metadata = packet->at(i);

        if (!isProlongedEvent(metadata->typeId()))
        {
            emitEvent(metadata, metadata->isActive(), packet->timestampUs());
            continue;
        }

        // Assume descriptions differ by region and only by region.
        auto key = std::tuple<QString, QString>(metadata->typeId(), metadata->description());
        auto& event = m_ongoingEvents[key];
        auto it = m_ongoingEvents.find(key);

        event.timer.cancelSync();

        emitEvent(metadata, true, packet->timestampUs());

        event.metadata = metadata;
        event.timestampDeltaUs = packet->timestampUs() - nowUs();
        event.timer.start(kTimeout,
            [this, it, &event]()
            {
                emitEvent(event.metadata, false, nowUs() + event.timestampDeltaUs);

                m_ongoingEvents.erase(it);

                for (auto& [key, event]: m_ongoingEvents)
                {
                    emitEvent(event.metadata, true, nowUs() + event.timestampDeltaUs);
                }
            });
    }
}

void EventProlonger::flush()
{
    for (auto& [key, event]: m_ongoingEvents)
    {
        event.timer.cancelSync();

        emitEvent(event.metadata, false, nowUs() + event.timestampDeltaUs);
    }
    m_ongoingEvents.clear();
}

cf::future<Ptr<IEventMetadataPacket>> EventProlonger::read()
{
    auto future = m_promise.emplace().get_future()
        .then(cf::translate_broken_promise_to_operation_canceled);

    pump();

    return future;
}

Ptr<IEventMetadataPacket> EventProlonger::readSync()
{
    if (m_packets.empty())
        return nullptr;

    auto packet = std::move(m_packets.front());
    m_packets.pop_front();
    return packet;
}

void EventProlonger::emitEvent(const Ptr<const IEventMetadata>& metadata,
    bool isActive, std::int64_t timestampUs)
{
    auto endMetadata = makePtr<EventMetadata>();

    endMetadata->setTypeId(metadata->typeId());
    endMetadata->setConfidence(metadata->confidence());
    endMetadata->setCaption(metadata->caption());
    endMetadata->setDescription(metadata->description());
    endMetadata->setIsActive(isActive);
    for (int i = 0; i < metadata->attributeCount(); ++i)
    {
        auto attribute = metadata->attribute(i);

        endMetadata->addAttribute(makePtr<Attribute>(
            attribute->type(),
            attribute->name(),
            attribute->value(),
            attribute->confidence()));
    }

    auto packet = makePtr<EventMetadataPacket>();

    packet->setTimestampUs(timestampUs);
    packet->addItem(endMetadata.get());

    m_packets.push_back(std::move(packet));

    pump();
}

void EventProlonger::pump()
{
    if (m_promise && !m_packets.empty())
    {
        auto promise = std::move(*m_promise);
        m_promise.reset();

        auto packet = std::move(m_packets.front());
        m_packets.pop_front();

        promise.set_value(std::move(packet));
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
