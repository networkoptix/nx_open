#include "event_parameters.h"

#include <QtCore/QVariant>

#include <nx/fusion/model_functions.h>
#include <utils/common/id.h>

namespace nx {
namespace vms {
namespace event {

EventParameters::EventParameters():
    eventType(EventType::undefinedEvent),
    eventTimestampUsec(0),
    reasonCode(EventReason::none)
{
}

QnUuid EventParameters::getParamsHash() const
{
    QByteArray paramKey(QByteArray::number(eventType));
    switch (eventType)
    {
        case EventType::serverFailureEvent:
        case EventType::storageFailureEvent:
            paramKey += '_' + QByteArray::number(int(reasonCode));
            if (reasonCode == EventReason::storageIoError
                || reasonCode == EventReason::storageTooSlow
                || reasonCode == EventReason::storageFull
                || reasonCode == EventReason::systemStorageFull
                || reasonCode == EventReason::metadataStorageOffline
                || reasonCode == EventReason::metadataStorageFull
                || reasonCode == EventReason::raidStorageError
                || reasonCode == EventReason::licenseRemoved)
            {
                paramKey += '_' + description.toUtf8();
            }
            break;

        case EventType::softwareTriggerEvent:
            return QnUuid::createUuid(); //< Warning: early return.
            break;

        case EventType::cameraInputEvent:
            paramKey += '_' + inputPortId.toUtf8();
            break;

        case EventType::cameraDisconnectEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            break;

        case EventType::networkIssueEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            paramKey += '_' + QByteArray::number(int(reasonCode));
            break;

        default:
            break;
    }

    return guidFromArbitraryData(paramKey);
}

QString EventParameters::getAnalyticsEventTypeId() const
{
    return inputPortId;
}

void EventParameters::setAnalyticsEventTypeId(const QString& id)
{
    inputPortId = id;
}

QnUuid EventParameters::getAnalyticsEngineId() const
{
    return analyticsEngineId;
}

void EventParameters::setAnalyticsEngineId(const QnUuid& id)
{
    analyticsEngineId = id;
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
