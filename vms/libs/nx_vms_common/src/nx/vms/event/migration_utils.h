// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>

#include <nx/vms/event/event_fwd.h>

namespace nx::vms::event {

inline const QMap<EventType, QString> eventTypesMap = {
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

inline const QMap<ActionType, QString> actionTypesMap = {
    {ActionType::acknowledgeAction, QStringLiteral("nx.actions.acknowledge")},
    {ActionType::bookmarkAction, QStringLiteral("nx.actions.bookmark")},
    {ActionType::buzzerAction, QStringLiteral("nx.actions.buzzer")},
    {ActionType::cameraOutputAction, QStringLiteral("nx.actions.deviceOutput")},
    {ActionType::cameraRecordingAction, QStringLiteral("nx.actions.deviceRecording")},
    {ActionType::diagnosticsAction, QStringLiteral("nx.actions.writeToLog")},
    {ActionType::execHttpRequestAction, QStringLiteral("nx.actions.http")},
    {ActionType::exitFullscreenAction, QStringLiteral("nx.actions.exitFullscreen")},
    {ActionType::fullscreenCameraAction, QStringLiteral("nx.actions.enterFullscreen")},
    {ActionType::openLayoutAction, QStringLiteral("nx.actions.openLayout")},
    {ActionType::panicRecordingAction, QStringLiteral("nx.actions.panicRecording")},
    {ActionType::playSoundOnceAction, QStringLiteral("nx.actions.playSound")},
    {ActionType::playSoundAction, QStringLiteral("nx.actions.repeatSound")},
    {ActionType::executePtzPresetAction, QStringLiteral("nx.actions.ptzPreset")},
    {ActionType::pushNotificationAction, QStringLiteral("nx.actions.pushNotification")},
    {ActionType::sendMailAction, QStringLiteral("nx.actions.sendEmail")},
    {ActionType::sayTextAction, QStringLiteral("nx.actions.speak")},
    {ActionType::showOnAlarmLayoutAction, QStringLiteral("nx.actions.showOnAlarmLayout")},
    {ActionType::showPopupAction, QStringLiteral("nx.actions.desktopNotification")},
    {ActionType::showTextOverlayAction, QStringLiteral("nx.actions.textOverlay")},
};

NX_VMS_COMMON_API EventType convertToOldEvent(const QString& typeId);
NX_VMS_COMMON_API QString convertToNewEvent(EventType eventType);

NX_VMS_COMMON_API QString convertToNewAction(ActionType actionType);

} // namespace nx::vms::event
