#include "business_help.h"

#include <ui/help/help_topics.h>

using namespace nx;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

// TODO: #vkutin Think of a proper namespace
namespace QnBusiness {

int eventHelpId(EventType type)
{
    switch (type)
    {
        case EventType::cameraMotionEvent:
            return Qn::EventsActions_CameraMotion_Help;
        case EventType::cameraInputEvent:
            return Qn::EventsActions_CameraInput_Help;
        case EventType::cameraDisconnectEvent:
            return Qn::EventsActions_CameraDisconnected_Help;
        case EventType::storageFailureEvent:
            return Qn::EventsActions_StorageFailure_Help;
        case EventType::networkIssueEvent:
            return Qn::EventsActions_NetworkIssue_Help;
        case EventType::cameraIpConflictEvent:
            return Qn::EventsActions_CameraIpConflict_Help;
        case EventType::serverFailureEvent:
            return Qn::EventsActions_MediaServerFailure_Help;
        case EventType::serverConflictEvent:
            return Qn::EventsActions_MediaServerConflict_Help;
        case EventType::serverStartEvent:
            return Qn::EventsActions_MediaServerStarted_Help;
        case EventType::licenseIssueEvent:
            return Qn::EventsActions_LicenseIssue_Help;
        case EventType::backupFinishedEvent:
            return Qn::EventsActions_BackupFinished_Help;
        case EventType::analyticsSdkEvent:
            return Qn::EventsActions_VideoAnalytics_Help;
        case EventType::softwareTriggerEvent:
            return Qn::EventActions_SoftTrigger_Help;
        default:
            return type >= EventType::userDefinedEvent
                ? Qn::EventsActions_Generic_Help
                : -1;
    }
}

int actionHelpId(ActionType type)
{
    switch (type)
    {
        case ActionType::cameraOutputAction:
            return Qn::EventsActions_CameraOutput_Help;
        case ActionType::cameraRecordingAction:
            return Qn::EventsActions_StartRecording_Help;
        case ActionType::panicRecordingAction:
            return Qn::EventsActions_StartPanicRecording_Help;
        case ActionType::sendMailAction:
            return Qn::EventsActions_SendMail_Help;
        case ActionType::showPopupAction:
            return Qn::EventsActions_ShowNotification_Help;
        case ActionType::playSoundOnceAction:
            return Qn::EventsActions_PlaySound_Help;
        case ActionType::playSoundAction:
            return Qn::EventsActions_PlaySoundRepeated_Help;
        case ActionType::sayTextAction:
            return Qn::EventsActions_Speech_Help;
        case ActionType::diagnosticsAction:
            return Qn::EventsActions_Diagnostics_Help;
        case ActionType::showOnAlarmLayoutAction:
            return Qn::EventsActions_ShowOnAlarmLayout_Help;
        case ActionType::bookmarkAction:
            return Qn::EventsActions_Bookmark_Help;
        case ActionType::executePtzPresetAction:
            return Qn::EventsActions_ExecutePtzPreset_Help;
        case ActionType::showTextOverlayAction:
            return Qn::EventsActions_ShowTextOverlay_Help;
        case ActionType::execHttpRequestAction:
            return Qn::EventsActions_ExecHttpRequest_Help;
        default:
            return -1;
    }
}

int healthHelpId(QnSystemHealth::MessageType type)
{
    switch(type)
    {
        case QnSystemHealth::EmailIsEmpty:
        case QnSystemHealth::UsersEmailIsEmpty:
            return Qn::EventsActions_EmailNotSet_Help;
        case QnSystemHealth::NoLicenses:
            return Qn::EventsActions_NoLicenses_Help;
        case QnSystemHealth::SmtpIsNotSet:
            return Qn::EventsActions_EmailServerNotSet_Help;
        case QnSystemHealth::EmailSendError:
            return Qn::EventsActions_SendMailError_Help;
        case QnSystemHealth::StoragesNotConfigured:
            return Qn::EventsActions_StoragesMisconfigured_Help;
        default:
            return -1;
    }
}

} // namespace QnBusiness
