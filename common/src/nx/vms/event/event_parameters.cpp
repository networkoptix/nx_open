#include "event_parameters.h"

#include <QtCore/QVariant>

#include <nx/fusion/model_functions.h>
#include <utils/common/id.h>

namespace nx {
namespace vms {
namespace event {

EventParameters::EventParameters():
    eventType(undefinedEvent),
    eventTimestampUsec(0),
    reasonCode(EventReason::none)
{
}

QnUuid EventParameters::getParamsHash() const
{
    QByteArray paramKey(QByteArray::number(eventType));
    switch (eventType)
    {
        case serverFailureEvent:
        case storageFailureEvent:
            paramKey += '_' + QByteArray::number(int(reasonCode));
            if (reasonCode == EventReason::storageIoError
                || reasonCode == EventReason::storageTooSlow
                || reasonCode == EventReason::storageFull
                || reasonCode == EventReason::systemStorageFull
                || reasonCode == EventReason::licenseRemoved)
            {
                paramKey += '_' + description.toUtf8();
            }
            break;

        case softwareTriggerEvent:
            return QnUuid::createUuid(); //< Warning: early return.
            break;

        case cameraInputEvent:
            paramKey += '_' + inputPortId.toUtf8();
            break;

        case cameraDisconnectEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            break;

        case networkIssueEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            paramKey += '_' + QByteArray::number(int(reasonCode));
            break;

        default:
            break;
    }

    return guidFromArbitraryData(paramKey);
}

QnUuid EventParameters::analyticsEventId() const
{
    return QnUuid::fromStringSafe(inputPortId);
}

void EventParameters::setAnalyticsEventId(const QnUuid& id)
{
    inputPortId = id.toString();
}

QnUuid EventParameters::analyticsDriverId() const
{
    return metadata.instigators.empty()
        ? QnUuid()
        : metadata.instigators.front();
}

void EventParameters::setAnalyticsDriverId(const QnUuid& id)
{
    metadata.instigators.clear();
    metadata.instigators.push_back(id);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (EventMetaData)(EventParameters),
    (ubjson)(json)(eq)(xml)(csv_record), _Fields, (brief, true))

bool checkForKeywords(const QString& value, const QString& keywords)
{
    if (keywords.trimmed().isEmpty())
        return true;

    for (const auto& keyword: nx::utils::smartSplit(keywords, L' ', QString::SkipEmptyParts))
    {
        if (value.contains(nx::utils::unquoteStr(keyword)))
            return true;
    }

    return false;
}

} // namespace event
} // namespace vms
} // namespace nx
