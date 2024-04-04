// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "migration_utils.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::event {

namespace {

// TODO: #amalov Use this mapping in migration code.
static const std::pair<EventType, QString> eventTypes[] = {
    {EventType::analyticsSdkEvent, QStringLiteral("nx.events.analytics")},
    {EventType::analyticsSdkObjectDetected, QStringLiteral("nx.events.analyticsObject")},
    {EventType::userDefinedEvent, QStringLiteral("nx.events.generic")},
    {EventType::cameraInputEvent, QStringLiteral("nx.events.cameraInput")},
    {EventType::cameraMotionEvent, QStringLiteral("nx.events.motion")},
    {EventType::pluginDiagnosticEvent, QStringLiteral("nx.events.pluginDiagnostic")},
    {EventType::softwareTriggerEvent, QStringLiteral("nx.events.softTrigger")},
    {EventType::cameraDisconnectEvent, QStringLiteral("nx.events.deviceDisconnected")},
    {EventType::cameraIpConflictEvent, QStringLiteral("nx.events.deviceIpConflict")},
    {EventType::fanErrorEvent, QStringLiteral("nx.events.fanError")},
    {EventType::ldapSyncIssueEvent, QStringLiteral("nx.events.ldapSyncIssue")},
    {EventType::licenseIssueEvent, QStringLiteral("nx.events.licenseIssue")},
    {EventType::saasIssueEvent, QStringLiteral("nx.events.saasIssue")},
    {EventType::networkIssueEvent, QStringLiteral("nx.events.networkIssue")},
    {EventType::poeOverBudgetEvent, QStringLiteral("nx.events.poeOverBudget")},
    {EventType::serverCertificateError, QStringLiteral("nx.events.serverCertificateError")},
    {EventType::serverConflictEvent, QStringLiteral("nx.events.serverConflict")},
    {EventType::serverFailureEvent, QStringLiteral("nx.events.serverFailure")},
    {EventType::storageFailureEvent, QStringLiteral("nx.events.storageIssue")},
    {EventType::serverStartEvent, QStringLiteral("nx.events.serverStarted")},
    {EventType::backupFinishedEvent, QStringLiteral("nx.events.archiveBackupFinished")},
};

} // namespace

EventType convertToOldEvent(const QString& typeId)
{
    static const auto eventTypeMap =
        []
        {
            QMap<QStringView, EventType> result;
            for(const auto& [oldType, newType]: eventTypes)
                result[newType] = oldType;

            return result;
        }();

    const auto result = eventTypeMap.value(typeId, EventType::undefinedEvent);
    NX_ASSERT(result != EventType::undefinedEvent);

    return result;
}

QString convertToNewEvent(const EventType& eventType)
{
    static const auto eventTypeMap =
        []
        {
            QMap<EventType, QStringView> result;
            for(const auto& [oldType, newType]: eventTypes)
                result[oldType] = newType;

            return result;
        }();

    const auto result = eventTypeMap.value(eventType);
    NX_ASSERT(!result.isEmpty());

    return result.toString();
}

} // namespace nx::vms::event
