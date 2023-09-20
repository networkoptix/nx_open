// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameters.h"

#include <QtCore/QVariant>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/reflect/json.h>
#include <utils/common/id.h>

#include "helpers.h"

namespace nx {
namespace vms {
namespace event {

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
                || reasonCode == EventReason::metadataStoragePermissionDenied
                || reasonCode == EventReason::raidStorageError
                || reasonCode == EventReason::encryptionFailed)
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

        case EventType::ldapSyncIssueEvent:
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

QString EventParameters::getAnalyticsObjectTypeId() const
{
    return inputPortId;
}

void EventParameters::setAnalyticsObjectTypeId(const QString& id)
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

QString EventParameters::getTriggerName() const
{
    return caption;
}

void EventParameters::setTriggerName(const QString& name)
{
    caption = name;
}

QString EventParameters::getTriggerIcon() const
{
    return description;
}

void EventParameters::setTriggerIcon(const QString& icon)
{
    description = icon;
}

nx::vms::api::EventLevels EventParameters::getDiagnosticEventLevels() const
{
    return nx::reflect::fromString<nx::vms::api::EventLevels>(inputPortId.toStdString());
}

void EventParameters::setDiagnosticEventLevels(nx::vms::api::EventLevels levels)
{
    inputPortId = QString::fromStdString(nx::reflect::toString(levels));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EventMetaData, (ubjson)(json)(xml)(csv_record), EventMetaData_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EventParameters, (ubjson)(json)(xml)(csv_record), EventParameters_Fields, (brief, true))

bool checkForKeywords(const QString& value, const QString& keywords)
{
    return checkForKeywords(value, splitOnPureKeywords(keywords));
}

} // namespace event
} // namespace vms
} // namespace nx
