#if defined(ENABLE_HANWHA)

#include "hanwha_bytestream_filter.h"
#include "hanwha_common.h"
#include "hanwha_string_helper.h"

#include <iostream>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

static const QString kChannelField = lit("channel");
static const QString kRegionField = lit("regionid");

static const QString kActive = lit("true");
static const QString kInactive = lit("false");

} // namespace

HanwhaBytestreamFilter::HanwhaBytestreamFilter(
    const Hanwha::DriverManifest& manifest,
    HanwhaBytestreamFilter::Handler handler)
    :
    m_manifest(manifest),
    m_handler(handler)
{
}

HanwhaBytestreamFilter::~HanwhaBytestreamFilter()
{

}

bool HanwhaBytestreamFilter::processData(const QnByteArrayConstRef& buffer)
{
    NX_ASSERT(m_handler, lit("No handler is set."));
    if (!m_handler)
        return false;

    m_handler(parseMetadataState(buffer));
    return true;
}

std::vector<HanwhaEvent> HanwhaBytestreamFilter::parseMetadataState(
    const QnByteArrayConstRef& buffer) const
{
    std::vector<HanwhaEvent> result;
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

boost::optional<HanwhaEvent> HanwhaBytestreamFilter::createEvent(
    const QString& eventSource,
    const QString& eventState) const
{
    auto eventTypeId = m_manifest.eventTypeByInternalName(eventSource);
    if (eventTypeId.isNull())
        return boost::none;

    HanwhaEvent event;
    event.typeId = nxpt::NxGuidHelper::fromRawData(eventTypeId.toRfc4122());
    event.channel = eventChannel(eventSource);
    event.region = eventRegion(eventSource);
    event.isActive = isEventActive(eventState);
    event.itemType = eventItemType(eventSource, eventState);
    event.fullEventName = eventSource;

    event.caption = HanwhaStringHelper::buildCaption(
        m_manifest,
        eventTypeId,
        event.channel,
        event.region,
        event.itemType,
        event.isActive);

    event.description = HanwhaStringHelper::buildDescription(
        m_manifest,
        eventTypeId,
        event.channel,
        event.region,
        event.itemType,
        event.isActive);

    return event;
}

boost::optional<int> HanwhaBytestreamFilter::eventChannel(const QString& eventSource) const
{
    auto split = eventSource.split(L'.');
    if (split.size() < 2)
        return boost::none;

    if (split[0] != kChannelField)
        return boost::none;

    bool success = false;
    int channel = split[1].toInt(&success);

    if (!success)
        return boost::none;

    return channel;
}

boost::optional<int> HanwhaBytestreamFilter::eventRegion(const QString& eventSource) const
{
    auto split = eventSource.split(L'.');
    auto splitSize = split.size();

    if (splitSize < 2)
        return boost::none;

    for (auto i = 0; i < splitSize; ++i)
    {
        if (split[i] == kRegionField && i < splitSize - 1)
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

bool HanwhaBytestreamFilter::isEventActive(const QString& eventSourceState) const
{
    qDebug() << "eventSourceState" << eventSourceState << (eventSourceState.toLower() == kActive);
    return eventSourceState == kActive;
}

Hanwha::EventItemType HanwhaBytestreamFilter::eventItemType(
    const QString& eventSource,
    const QString& eventState) const
{
    return Hanwha::EventItemType::none;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

#endif // defined(ENABLE_HANWHA)
