#include "business_help.h"

#include <ui/help/help_topics.h>

using namespace nx;

// TODO: #vkutin Think of a proper namespace
namespace QnBusiness {

int eventHelpId(vms::event::EventType type)
{
    switch (type)
    {
        case vms::event::cameraMotionEvent:
            return Qn::EventsActions_CameraMotion_Help;
        case vms::event::cameraInputEvent:
            return Qn::EventsActions_CameraInput_Help;
        case vms::event::cameraDisconnectEvent:
            return Qn::EventsActions_CameraDisconnected_Help;
        case vms::event::storageFailureEvent:
            return Qn::EventsActions_StorageFailure_Help;
        case vms::event::networkIssueEvent:
            return Qn::EventsActions_NetworkIssue_Help;
        case vms::event::cameraIpConflictEvent:
            return Qn::EventsActions_CameraIpConflict_Help;
        case vms::event::serverFailureEvent:
            return Qn::EventsActions_MediaServerFailure_Help;
        case vms::event::serverConflictEvent:
            return Qn::EventsActions_MediaServerConflict_Help;
        case vms::event::serverStartEvent:
            return Qn::EventsActions_MediaServerStarted_Help;
        case vms::event::licenseIssueEvent:
            return Qn::EventsActions_LicenseIssue_Help;
        case vms::event::backupFinishedEvent:
            return Qn::EventsActions_BackupFinished_Help;
        case vms::event::analyticsSdkEvent:
            return Qn::EventsActions_VideoAnalytics_Help;
        default:
            return type >= vms::event::userDefinedEvent
                ? Qn::EventsActions_Generic_Help
                : -1;
    }
}

int actionHelpId(vms::event::ActionType type)
{
    switch (type)
    {
        case vms::event::cameraOutputAction:
            return Qn::EventsActions_CameraOutput_Help;
        case vms::event::cameraRecordingAction:
            return Qn::EventsActions_StartRecording_Help;
        case vms::event::panicRecordingAction:
            return Qn::EventsActions_StartPanicRecording_Help;
        case vms::event::sendMailAction:
            return Qn::EventsActions_SendMail_Help;
        case vms::event::showPopupAction:
            return Qn::EventsActions_ShowNotification_Help;
        case vms::event::playSoundOnceAction:
            return Qn::EventsActions_PlaySound_Help;
        case vms::event::playSoundAction:
            return Qn::EventsActions_PlaySoundRepeated_Help;
        case vms::event::sayTextAction:
            return Qn::EventsActions_Speech_Help;
        case vms::event::diagnosticsAction:
            return Qn::EventsActions_Diagnostics_Help;
        case vms::event::showOnAlarmLayoutAction:
            return Qn::EventsActions_ShowOnAlarmLayout_Help;
        case vms::event::bookmarkAction:
            return Qn::EventsActions_Bookmark_Help;
        case vms::event::executePtzPresetAction:
            return Qn::EventsActions_ExecutePtzPreset_Help;
        case vms::event::showTextOverlayAction:
            return Qn::EventsActions_ShowTextOverlay_Help;
        case vms::event::execHttpRequestAction:
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
        case QnSystemHealth::StoragesAreFull:
            return Qn::EventsActions_StorageFull_Help;
        default:
            return -1;
    }
}

} // namespace QnBusiness
