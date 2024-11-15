// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>

#include <nx/vms/event/event_fwd.h>

namespace nx::vms::event {

inline const QMap<EventType, QString> eventTypesMap = {
    {EventType::analyticsSdkEvent, QStringLiteral("analytics")},
    {EventType::analyticsSdkObjectDetected, QStringLiteral("analyticsObject")},
    {EventType::userDefinedEvent, QStringLiteral("generic")},
    {EventType::cameraInputEvent, QStringLiteral("cameraInput")},
    {EventType::cameraMotionEvent, QStringLiteral("motion")},
    {EventType::pluginDiagnosticEvent, QStringLiteral("pluginDiagnostic")},
    {EventType::softwareTriggerEvent, QStringLiteral("softTrigger")},
    {EventType::cameraDisconnectEvent, QStringLiteral("deviceDisconnected")},
    {EventType::cameraIpConflictEvent, QStringLiteral("deviceIpConflict")},
    {EventType::fanErrorEvent, QStringLiteral("fanError")},
    {EventType::ldapSyncIssueEvent, QStringLiteral("ldapSyncIssue")},
    {EventType::licenseIssueEvent, QStringLiteral("licenseIssue")},
    {EventType::saasIssueEvent, QStringLiteral("saasIssue")},
    {EventType::networkIssueEvent, QStringLiteral("networkIssue")},
    {EventType::poeOverBudgetEvent, QStringLiteral("poeOverBudget")},
    {EventType::serverCertificateError, QStringLiteral("serverCertificateError")},
    {EventType::serverConflictEvent, QStringLiteral("serverConflict")},
    {EventType::serverFailureEvent, QStringLiteral("serverFailure")},
    {EventType::storageFailureEvent, QStringLiteral("storageIssue")},
    {EventType::serverStartEvent, QStringLiteral("serverStarted")},
    {EventType::backupFinishedEvent, QStringLiteral("archiveBackupFinished")},
};

inline const QMap<ActionType, QString> actionTypesMap = {
    {ActionType::acknowledgeAction, QStringLiteral("acknowledge")},
    {ActionType::bookmarkAction, QStringLiteral("bookmark")},
    {ActionType::buzzerAction, QStringLiteral("buzzer")},
    {ActionType::cameraOutputAction, QStringLiteral("deviceOutput")},
    {ActionType::cameraRecordingAction, QStringLiteral("deviceRecording")},
    {ActionType::diagnosticsAction, QStringLiteral("writeToLog")},
    {ActionType::execHttpRequestAction, QStringLiteral("http")},
    {ActionType::exitFullscreenAction, QStringLiteral("exitFullscreen")},
    {ActionType::fullscreenCameraAction, QStringLiteral("enterFullscreen")},
    {ActionType::openLayoutAction, QStringLiteral("openLayout")},
    {ActionType::panicRecordingAction, QStringLiteral("panicRecording")},
    {ActionType::playSoundOnceAction, QStringLiteral("playSound")},
    {ActionType::playSoundAction, QStringLiteral("repeatSound")},
    {ActionType::executePtzPresetAction, QStringLiteral("ptzPreset")},
    {ActionType::pushNotificationAction, QStringLiteral("pushNotification")},
    {ActionType::sendMailAction, QStringLiteral("sendEmail")},
    {ActionType::sayTextAction, QStringLiteral("speak")},
    {ActionType::showOnAlarmLayoutAction, QStringLiteral("showOnAlarmLayout")},
    {ActionType::showPopupAction, QStringLiteral("desktopNotification")},
    {ActionType::showTextOverlayAction, QStringLiteral("textOverlay")},
};

NX_VMS_COMMON_API EventType convertToOldEvent(const QString& typeId);
NX_VMS_COMMON_API QString convertToNewEvent(EventType eventType);

NX_VMS_COMMON_API QString convertToNewAction(ActionType actionType);

} // namespace nx::vms::event
