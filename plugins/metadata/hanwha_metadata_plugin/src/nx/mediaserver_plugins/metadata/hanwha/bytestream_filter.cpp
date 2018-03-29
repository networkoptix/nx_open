#include "bytestream_filter.h"

#include <iostream>

#include "common.h"
#include "string_helper.h"
#include <nx/utils/log/log.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

namespace {

static const QString kChannelField = lit("channel");
static const QString kRegionField = lit("regionid");

static const QString kActive = lit("true");
static const QString kInactive = lit("false");

} // namespace

BytestreamFilter::BytestreamFilter(
    const Hanwha::DriverManifest& manifest,
    Handler handler)
    :
    m_manifest(manifest),
    m_handler(handler)
{
}

BytestreamFilter::~BytestreamFilter()
{
}

bool BytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    NX_ASSERT(m_handler, lit("No handler is set."));
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

    for (const auto& entry: split)
    {
        auto trimmed = entry.trimmed();
        auto nameAndValue = trimmed.split(L'=');

        if (nameAndValue.size() != 2)
            continue;

        auto event = createEvent(
            QString::fromUtf8(nameAndValue[0]).trimmed(),
            QString::fromUtf8(nameAndValue[1]).toLower().trimmed());

        if (event.is_initialized())
            result.push_back(*event);
    }

    return result;
}

boost::optional<Event> BytestreamFilter::createEvent(
    const QString& eventSource,
    const QString& eventState) const
{
    using namespace nx::api;

    auto eventTypeId = m_manifest.eventTypeByInternalName(eventSource);
    if (eventTypeId.isNull())
        return boost::none;
    const auto eventDescriptor = m_manifest.eventDescriptorById(eventTypeId);

    Event event;
    event.typeId = nxpt::NxGuidHelper::fromRawData(eventTypeId.toRfc4122());
    event.channel = eventChannel(eventSource);
    event.region = eventRegion(eventSource);
    if (eventDescriptor.flags.testFlag(Analytics::EventTypeFlag::stateDependent))
        event.isActive = isEventActive(eventState);  //< Event start/stop.
    else
        event.isActive = true; //< Event occurred.
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

/*static*/ boost::optional<int> BytestreamFilter::eventChannel(const QString& eventSource)
{
    auto split = eventSource.split(L'.');
    if (split.size() < 2)
        return boost::none;

    if (split[0].toLower() != kChannelField)
        return boost::none;

    bool success = false;
    int channel = split[1].toInt(&success);

    if (!success)
        return boost::none;

    return channel;
}

/*static*/ boost::optional<int> BytestreamFilter::eventRegion(const QString& eventSource)
{
    auto split = eventSource.split(L'.');
    auto splitSize = split.size();

    if (splitSize < 2)
        return boost::none;

    for (auto i = 0; i < splitSize; ++i)
    {
        if (split[i].toLower() == kRegionField && i < splitSize - 1)
        {
            bool success = false;
            int region = split[i + 1].toInt(&success);

            if (!success)
                return boost::none;

            return region;
        }
    }

    return boost::none;
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
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

