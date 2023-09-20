// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rules_help.h"

#include "help_topic.h"

namespace nx::vms::client::desktop {

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace rules {

int eventHelpId(EventType type)
{
    switch (type)
    {
        case EventType::cameraMotionEvent:
            return HelpTopic::Id::EventsActions_CameraMotion;
        case EventType::cameraInputEvent:
            return HelpTopic::Id::EventsActions_CameraInput;
        case EventType::cameraDisconnectEvent:
            return HelpTopic::Id::EventsActions_CameraDisconnected;
        case EventType::storageFailureEvent:
            return HelpTopic::Id::EventsActions_StorageFailure;
        case EventType::networkIssueEvent:
            return HelpTopic::Id::EventsActions_NetworkIssue;
        case EventType::cameraIpConflictEvent:
            return HelpTopic::Id::EventsActions_CameraIpConflict;
        case EventType::serverFailureEvent:
        case EventType::serverCertificateError:
            return HelpTopic::Id::EventsActions_MediaServerFailure;
        case EventType::serverConflictEvent:
            return HelpTopic::Id::EventsActions_MediaServerConflict;
        case EventType::serverStartEvent:
            return HelpTopic::Id::EventsActions_MediaServerStarted;
        case EventType::ldapSyncIssueEvent:
            return HelpTopic::Id::Ldap; //< #spanasenko What is a proper page?
        case EventType::licenseIssueEvent:
            return HelpTopic::Id::EventsActions_LicenseIssue;
        case EventType::backupFinishedEvent:
            return HelpTopic::Id::EventsActions_BackupFinished;
        case EventType::analyticsSdkEvent:
            return HelpTopic::Id::EventsActions_VideoAnalytics;
        case EventType::analyticsSdkObjectDetected:
            return HelpTopic::Id::EventsActions_VideoAnalytics;
        case EventType::softwareTriggerEvent:
            return HelpTopic::Id::EventsActions_SoftTrigger;
        default:
            return type >= EventType::userDefinedEvent
                ? HelpTopic::Id::EventsActions_Generic
                : -1;
    }
}

int actionHelpId(ActionType type)
{
    switch (type)
    {
        case ActionType::cameraOutputAction:
            return HelpTopic::Id::EventsActions_CameraOutput;
        case ActionType::cameraRecordingAction:
            return HelpTopic::Id::EventsActions_StartRecording;
        case ActionType::panicRecordingAction:
            return HelpTopic::Id::EventsActions_StartPanicRecording;
        case ActionType::sendMailAction:
            return HelpTopic::Id::EventsActions_SendMail;
        case ActionType::showPopupAction:
            return HelpTopic::Id::EventsActions_ShowDesktopNotification;
        case ActionType::pushNotificationAction:
            return HelpTopic::Id::EventsActions_SendMobileNotification;
        case ActionType::playSoundOnceAction:
            return HelpTopic::Id::EventsActions_PlaySound;
        case ActionType::playSoundAction:
            return HelpTopic::Id::EventsActions_PlaySoundRepeated;
        case ActionType::sayTextAction:
            return HelpTopic::Id::EventsActions_Speech;
        case ActionType::diagnosticsAction:
            return HelpTopic::Id::EventsActions_Diagnostics;
        case ActionType::showOnAlarmLayoutAction:
            return HelpTopic::Id::EventsActions_ShowOnAlarmLayout;
        case ActionType::showIntercomInformer:
            return HelpTopic::Id::EventsActions_ShowIntercomInformer;
        case ActionType::bookmarkAction:
            return HelpTopic::Id::EventsActions_Bookmark;
        case ActionType::executePtzPresetAction:
            return HelpTopic::Id::EventsActions_ExecutePtzPreset;
        case ActionType::showTextOverlayAction:
            return HelpTopic::Id::EventsActions_ShowTextOverlay;
        case ActionType::execHttpRequestAction:
            return HelpTopic::Id::EventsActions_ExecHttpRequest;
        default:
            return -1;
    }
}

int healthHelpId(nx::vms::common::system_health::MessageType type)
{
    using nx::vms::common::system_health::MessageType;

    switch (type)
    {
        case MessageType::emailIsEmpty:
        case MessageType::usersEmailIsEmpty:
            return HelpTopic::Id::EventsActions_EmailNotSet;
        case MessageType::noLicenses:
            return HelpTopic::Id::EventsActions_NoLicenses;
        case MessageType::smtpIsNotSet:
            return HelpTopic::Id::EventsActions_EmailServerNotSet;
        case MessageType::emailSendError:
            return HelpTopic::Id::EventsActions_SendMailError;
        case MessageType::storagesNotConfigured:
            return HelpTopic::Id::EventsActions_StoragesMisconfigured;
        case MessageType::backupStoragesNotConfigured:
            return HelpTopic::Id::ServerSettings_StoragesBackup;
        default:
            return -1;
    }
}

} // namespace rules

} // namespace nx::vms::client::desktop
