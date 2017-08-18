#include "hanwha_string_helper.h"
#include "hanwha_common.h"

#include <nx/utils/literal.h>
#include <nx/utils/std/hashes.h>

#include <plugins/plugin_tools.h>

namespace nx {
namespace mediaserver {
namespace plugins {

using TemplateFlags = HanwhaStringHelper::TemplateFlags;
using TemplateFlag = HanwhaStringHelper::TemplateFlag;

namespace {

static const std::unordered_map<nxpl::NX_GUID, QString> kCaptionTemplates = {
    {kHanwhaFaceDetectionEventId, lit("Face detection")},
    {kHanwhaVirtualLineEventId, lit("Virtual line crossing")},
    {kHanwhaEnteringEventId, lit("Entering the area")},
    {kHanwhaExitingEventId, lit("Exiting the area")},
    {kHanwhaAppearingEventId, lit("Appearing in the area")},
    {kHanwhaDisappearingEventId, lit("Disappearing from the area")},
    {kHanwhaAudioDetectionEventId, lit("Audio detection")},
    {kHanwhaTamperingEventId, lit("Tampering detection")},
    {kHanwhaDefocusingEventId, lit("Defocusing detection")},
    {kHanwhaDryContactInputEventId, lit("Dry contact input")},
    {kHanwhaMotionDetectionEventId, lit("Motion detection")},
    {kHanwhaSoundClassificationEventId, lit("Sound classification")},
    {kHanwhaLoiteringEventId, lit("Loitering detecetion")}
};

static const std::unordered_map<nxpl::NX_GUID, QString> kDescriptionTemplates = {
    {kHanwhaFaceDetectionEventId, lit("Face detected")},
    {kHanwhaVirtualLineEventId, lit("Virtual line #%1 was crossed")},
    {kHanwhaEnteringEventId, lit("Object has entered%1")},
    {kHanwhaExitingEventId, lit("Object has exited%1")},
    {kHanwhaAppearingEventId, lit("Object has appeared%1")},
    {kHanwhaDisappearingEventId, lit("Object has disappeared%1")},
    {kHanwhaAudioDetectionEventId, lit("The audio volume level is %1 the threshold")},
    {kHanwhaTamperingEventId, lit("Tampering %1")},
    {kHanwhaDefocusingEventId, lit("Defocusing %1")},
    {kHanwhaDryContactInputEventId, lit("Dry contact input has been %1")},
    {kHanwhaMotionDetectionEventId, lit("Motion %1%2")},
    {kHanwhaSoundClassificationEventId, lit("Sound of type %1 has been detected")},
    {kHanwhaLoiteringEventId, lit("Loitering %1%2")}
};

static const std::unordered_map<nxpl::NX_GUID, QString> kPositiveStateTemplates = {
    {kHanwhaAudioDetectionEventId, lit("below")},
    {kHanwhaTamperingEventId, lit("has been detected")},
    {kHanwhaDefocusingEventId, lit("has been detected")},
    {kHanwhaDryContactInputEventId, lit("activated")},
    {kHanwhaMotionDetectionEventId, lit("has been detected")},
    {kHanwhaLoiteringEventId, lit("has been detected")}
};

static const std::unordered_map<nxpl::NX_GUID, QString> kNegativeStateTemplates = {
    {kHanwhaAudioDetectionEventId, lit("above")},
    {kHanwhaTamperingEventId, lit("has ended")},
    {kHanwhaDefocusingEventId, lit("has ended")},
    {kHanwhaDryContactInputEventId, lit("deactivated")},
    {kHanwhaMotionDetectionEventId, lit("has ended")},
    {kHanwhaLoiteringEventId, lit("has ended")}
};

static const std::unordered_map<nxpl::NX_GUID, QString> kRegionTemplates = {
    {kHanwhaVirtualLineEventId, lit("#%1")},
    {kHanwhaEnteringEventId, lit(" the region #%1")},
    {kHanwhaExitingEventId, lit(" th region #%1")},
    {kHanwhaAppearingEventId, lit(" in the region #%1")},
    {kHanwhaDisappearingEventId, lit(" from the region #%1")},
    {kHanwhaMotionDetectionEventId, lit(" in the region #%1")},
    {kHanwhaLoiteringEventId, lit(" in the region #%1")}
};

static const std::unordered_map<HanwhaEventItemType, QString> kEventItemTypeStrings = {
    {HanwhaEventItemType::ein, "One"},
    {HanwhaEventItemType::zwei, "Two"},
    {HanwhaEventItemType::drei, "Three"},
    {HanwhaEventItemType::none, "None"}
};

static const std::unordered_map<nxpl::NX_GUID, TemplateFlags> kDescriptionTemplateFlags = {
    {kHanwhaFaceDetectionEventId, TemplateFlags()},
    {kHanwhaVirtualLineEventId, TemplateFlag::regionDependent},
    {kHanwhaEnteringEventId, TemplateFlag::regionDependent},
    {kHanwhaExitingEventId, TemplateFlag::regionDependent},
    {kHanwhaAppearingEventId, TemplateFlag::regionDependent},
    {kHanwhaDisappearingEventId, TemplateFlag::regionDependent},
    {kHanwhaAudioDetectionEventId, TemplateFlag::stateDependent},
    {kHanwhaTamperingEventId, TemplateFlag::stateDependent},
    {kHanwhaDefocusingEventId, TemplateFlag::stateDependent},
    {kHanwhaDryContactInputEventId, TemplateFlag::stateDependent},
    {kHanwhaMotionDetectionEventId, TemplateFlag::stateDependent | TemplateFlag::regionDependent},
    {kHanwhaSoundClassificationEventId, TemplateFlag::itemDependent},
    {kHanwhaLoiteringEventId, TemplateFlag::stateDependent | TemplateFlag::regionDependent}
};

static const std::unordered_map<QString, nxpl::NX_GUID> kEventNameToEventType = {
    {lit("MotionDetection"), kHanwhaMotionDetectionEventId},
    {lit("Tampering"), kHanwhaTamperingEventId},
    {lit("DefocusDetection"), kHanwhaDefocusingEventId}
};

} // namespace 


QString HanwhaStringHelper::buildCaption(
    const nxpl::NX_GUID& eventType,
    boost::optional<int> eventChannel,
    boost::optional<int> eventRegion,
    HanwhaEventItemType eventItemType,
    bool isActive)
{
    auto captionTemplate = findInMap(eventType, kCaptionTemplates);
    if (captionTemplate)
        return *captionTemplate;

    return QString();
}

QString HanwhaStringHelper::buildDescription(
    const nxpl::NX_GUID& eventType,
    boost::optional<int> eventChannel,
    boost::optional<int> eventRegion,
    HanwhaEventItemType eventItemType,
    bool isActive)
{
    auto mainTemplate = descriptionTemplate(eventType);
    if (!mainTemplate)
        return QString();

    auto flags = templateFlags(eventType);

    if (flags.testFlag(TemplateFlag::stateDependent))
    {
        auto stateStrTemplate = stateStringTemplate(eventType, isActive);
        if (stateStrTemplate)
            mainTemplate = mainTemplate->arg(*stateStrTemplate);
    }

    if (flags.testFlag(TemplateFlag::regionDependent))
    {
        auto regionStrTemplate = regionStringTemplate(eventType, isActive);
        if (regionStrTemplate && eventRegion)
        {
            regionStrTemplate = regionStrTemplate->arg(*eventRegion);
            mainTemplate = mainTemplate->arg(*regionStrTemplate);
        }
        else
        {
            mainTemplate = mainTemplate->arg(QString());
        }
    }

    if (flags.testFlag(TemplateFlag::itemDependent))
    {
        auto itemStrTemplate = itemStringTemplate(eventItemType);
        mainTemplate = mainTemplate->arg(itemStrTemplate);
    }

    return *mainTemplate;
}

boost::optional<nxpl::NX_GUID> HanwhaStringHelper::fromStringToEventType(const QString& eventName)
{
    return findInMap(eventName, kEventNameToEventType);
}

boost::optional<QString> HanwhaStringHelper::descriptionTemplate(const nxpl::NX_GUID& eventType)
{
    return findInMap(eventType, kDescriptionTemplates);
}

boost::optional<QString> HanwhaStringHelper::stateStringTemplate(const nxpl::NX_GUID& eventType, bool isActive)
{
    if (isActive)
        return findInMap(eventType, kPositiveStateTemplates);
    else
        return findInMap(eventType, kNegativeStateTemplates);
}

boost::optional<QString> HanwhaStringHelper::regionStringTemplate(const nxpl::NX_GUID& eventType, bool /*eventState*/)
{
    return findInMap(eventType, kRegionTemplates);
}

QString HanwhaStringHelper::itemStringTemplate(HanwhaEventItemType eventItemType)
{
    auto itemStr = findInMap(eventItemType, kEventItemTypeStrings);
    if (!itemStr)
        return QString();

    return *itemStr;
}

TemplateFlags HanwhaStringHelper::templateFlags(const nxpl::NX_GUID& eventType)
{
    auto flags = findInMap(eventType, kDescriptionTemplateFlags);
    if (flags.is_initialized())
        return *flags;

    return TemplateFlags();
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx