#include "hanwha_bytestream_filter.h"
#include "hanwha_common.h"
#include "hanwha_string_helper.h"

#include <iostream>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

static const QString kHanwhaFaceDetectionString = lit("facedetection");
static const QString kHanwhaVirtualLineString = lit("virtualline");
static const QString kHanwhaEnteringString = lit("enter");
static const QString kHanwhaExitingString = lit("exit");
static const QString kHanwhaAppearingString = lit("appear");
static const QString kHanwhaDisappearingString = lit("disappear");
static const QString kHanwhaAudioDetectionString = lit("audiodetection");
static const QString kHanwhaTamperingString = lit("tampering");
static const QString kHanwhaDefocusString = lit("defocusdetection");
static const QString kHanwhaDryContactString = lit("drycontact");
static const QString kHanwhaMotionDetectionString = lit("motiondetection");
static const QString kHanwhaSoundClassificationString = lit("sound");
static const QString kHanwhaLoiteringString = lit("loitering");

static const QString kChannelField = lit("channel");
static const QString kRegionField = lit("regionid");

static const QString kActive = lit("true");
static const QString kInactive = lit("false");

} // namespace

HanwhaBytestreamFilter::HanwhaBytestreamFilter(HanwhaBytestreamFilter::Handler handler):
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
            QString::fromUtf8(nameAndValue[0]).toLower().trimmed(),
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
    HanwhaEvent event;
    auto typeId = eventTypeId(eventSource);
    if (!typeId.is_initialized())
        return boost::none;

    event.typeId = *typeId;
    event.channel = eventChannel(eventSource);
    event.region = eventRegion(eventSource);
    event.isActive = isEventActive(eventState);
    event.itemType = eventItemType(eventSource, eventState);
    
    fillCaption(&event);
    fillDescription(&event);

    return event;
}

boost::optional<nxpl::NX_GUID> HanwhaBytestreamFilter::eventTypeId(const QString& eventSource) const
{
    if (eventSource.contains(kHanwhaFaceDetectionString))
        return kHanwhaFaceDetectionEventId;
    else if (eventSource.contains(kHanwhaVirtualLineString))
        return kHanwhaVirtualLineEventId;
    else if (eventSource.contains(kHanwhaEnteringString))
        return kHanwhaEnteringEventId;
    else if (eventSource.contains(kHanwhaExitingString))
        return kHanwhaExitingEventId;
    else if (eventSource.contains(kHanwhaAppearingString))
        return kHanwhaAppearingEventId;
    else if (eventSource.contains(kHanwhaDisappearingString))
        return kHanwhaDisappearingEventId;
    else if (eventSource.contains(kHanwhaAudioDetectionString))
        return kHanwhaAudioDetectionEventId;
    else if (eventSource.contains(kHanwhaTamperingString))
        return kHanwhaTamperingEventId;
    else if (eventSource.contains(kHanwhaDefocusString))
        return kHanwhaDefocusingEventId;
    else if (eventSource.contains(kHanwhaDryContactString))
        return kHanwhaDryContactInputEventId;
    else if (eventSource.contains(kHanwhaMotionDetectionString) && !eventSource.contains("regionid"))
        return kHanwhaMotionDetectionEventId;
    else if (eventSource.contains(kHanwhaSoundClassificationString))
        return kHanwhaSoundClassificationEventId;
    else if (eventSource.contains(kHanwhaLoiteringString))
        return kHanwhaLoiteringEventId;

    return boost::none;
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

HanwhaEventItemType HanwhaBytestreamFilter::eventItemType(
    const QString& eventSource,
    const QString& eventState) const
{
    return HanwhaEventItemType::none;
}

void HanwhaBytestreamFilter::fillCaption(HanwhaEvent* inOutEvent) const
{
    NX_ASSERT(inOutEvent);
    inOutEvent->caption = HanwhaStringHelper::buildCaption(
        inOutEvent->typeId,
        inOutEvent->channel,
        inOutEvent->region,
        inOutEvent->itemType,
        inOutEvent->isActive);
}

void HanwhaBytestreamFilter::fillDescription(HanwhaEvent* inOutEvent) const
{
    NX_ASSERT(inOutEvent);
    inOutEvent->description = HanwhaStringHelper::buildDescription(
        inOutEvent->typeId,
        inOutEvent->channel,
        inOutEvent->region,
        inOutEvent->itemType,
        inOutEvent->isActive);
}


} // namespace plugins
} // namespace mediaserver
} // namespace nx
