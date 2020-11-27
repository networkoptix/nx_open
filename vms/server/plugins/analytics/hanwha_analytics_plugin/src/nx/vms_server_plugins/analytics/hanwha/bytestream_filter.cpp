#include "bytestream_filter.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/algorithm.h>

#include "common.h"
#include "string_helper.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

namespace {

static const QString kChannelField("channel");
static const std::set<QString> kRegionFields = {"regionid", "queue", "definedareaid", "lineid"};

static const QString kActive("true");
static const QString kInactive("false");

} // namespace

BytestreamFilter::BytestreamFilter(
    const Hanwha::EngineManifest& manifest,
    Handler handler)
    :
    m_manifest(manifest),
    m_handler(handler)
{
}

bool BytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    NX_ASSERT(m_handler, "No handler is set.");
    if (!m_handler)
        return false;

    m_handler(parseMetadataState(buffer));
    return true;
}

std::vector<Event> BytestreamFilter::parseMetadataState(
    const QnByteArrayConstRef& buffer) const
{
    std::vector<Event> result;
    auto split = buffer.split(L'\n');

    std::set<QString> regionedTypeIds;
    for (const auto& entry: split)
    {
        auto trimmed = entry.trimmed();
        auto nameAndValue = trimmed.split(L'=');

        if (nameAndValue.size() != 2)
            continue;

        auto event = createEvent(
            QString::fromUtf8(nameAndValue[0]).trimmed(),
            QString::fromUtf8(nameAndValue[1]).toLower().trimmed());

        if (event)
        {
            result.push_back(*event);
            if (event->region)
                regionedTypeIds.insert(event->typeId);
        }
    }

    // Camera sends duplicate events, with and without region id, so for each event type we remove
    // ones that have no region id if any are present with region id.
    nx::utils::remove_if(result,
        [&](auto& event) { return !event.region && regionedTypeIds.count(event.typeId); });

    return result;
}

std::optional<Event> BytestreamFilter::createEvent(
    const QString& eventSource,
    const QString& eventState) const
{
    using namespace nx::vms::api::analytics;

    auto eventTypeId = m_manifest.eventTypeIdByName(eventSource);
    if (eventTypeId.isEmpty())
        return std::nullopt;
    const auto eventTypeDescriptor = m_manifest.eventTypeDescriptorById(eventTypeId);

    Event event;
    event.typeId = eventTypeId;
    event.channel = eventChannel(eventSource);
    event.region = eventRegion(eventSource);
    event.isActive = isEventActive(eventState); //< Event start/stop.
    if (!event.isActive
        && !eventTypeDescriptor.flags.testFlag(EventTypeFlag::stateDependent))
    {
        return {};
    }

    event.itemType = eventItemType(eventSource, eventState);
    event.fullEventName = eventSource;

    event.caption = StringHelper::buildCaption(
        m_manifest,
        eventTypeId,
        event.channel,
        event.region,
        event.itemType,
        event.isActive);

    event.description = StringHelper::buildDescription(
        m_manifest,
        eventTypeId,
        event.channel,
        event.region,
        event.itemType,
        event.isActive);

    return event;
}

/*static*/ std::optional<int> BytestreamFilter::eventChannel(const QString& eventSource)
{
    auto split = eventSource.split(L'.');
    if (split.size() < 2)
        return std::nullopt;

    if (split[0].toLower() != kChannelField)
        return std::nullopt;

    bool success = false;
    int channel = split[1].toInt(&success);

    if (!success)
        return std::nullopt;

    return channel;
}

/*static*/ std::optional<int> BytestreamFilter::eventRegion(const QString& eventSource)
{
    auto split = eventSource.split(L'.');
    auto splitSize = split.size();

    if (splitSize < 2)
        return std::nullopt;

    for (auto i = 0; i < splitSize; ++i)
    {
        if (kRegionFields.find(split[i].toLower()) != kRegionFields.cend() && i < splitSize - 1)
        {
            bool success = false;
            int region = split[i + 1].toInt(&success);

            if (!success)
                return std::nullopt;

            return region;
        }
    }

    return std::nullopt;
}

/*static*/ bool BytestreamFilter::isEventActive(const QString& eventSourceState)
{
    bool isActive = (eventSourceState == kActive);
    NX_VERBOSE(typeid(BytestreamFilter), lm("eventSourceState = '%1' -> %2").args(eventSourceState, isActive));
    return isActive;
}

/*static*/ Hanwha::EventItemType BytestreamFilter::eventItemType(
    const QString& eventSource, const QString& eventState)
{
    return Hanwha::EventItemType::none;
}

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
