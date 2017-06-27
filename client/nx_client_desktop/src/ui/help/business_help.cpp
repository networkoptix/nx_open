#include "business_help.h"

#include <ui/help/help_topics.h>

using namespace nx;

// TODO: #vkutin Think of a proper namespace
namespace QnBusiness {

int eventHelpId(vms::event::EventType type)
{
    switch (type)
    {
        case vms::event::CameraMotionEvent:
            return Qn::EventsActions_CameraMotion_Help;
        case vms::event::CameraInputEvent:
            return Qn::EventsActions_CameraInput_Help;
        case vms::event::CameraDisconnectEvent:
            return Qn::EventsActions_CameraDisconnected_Help;
        case vms::event::StorageFailureEvent:
            return Qn::EventsActions_StorageFailure_Help;
        case vms::event::NetworkIssueEvent:
            return Qn::EventsActions_NetworkIssue_Help;
        case vms::event::CameraIpConflictEvent:
            return Qn::EventsActions_CameraIpConflict_Help;
        case vms::event::ServerFailureEvent:
            return Qn::EventsActions_MediaServerFailure_Help;
        case vms::event::ServerConflictEvent:
            return Qn::EventsActions_MediaServerConflict_Help;
        case vms::event::ServerStartEvent:
            return Qn::EventsActions_MediaServerStarted_Help;
        case vms::event::LicenseIssueEvent:
            return Qn::EventsActions_LicenseIssue_Help;
        case vms::event::BackupFinishedEvent:
            return Qn::EventsActions_BackupFinished_Help;
        default:
            return type >= vms::event::UserDefinedEvent
                ? Qn::EventsActions_Generic_Help
                : -1;
    }
}

int actionHelpId(vms::event::ActionType type)
{
    switch (type)
    {
        case vms::event::CameraOutputAction:
            return Qn::EventsActions_CameraOutput_Help;
        case vms::event::CameraRecordingAction:
            return Qn::EventsActions_StartRecording_Help;
        case vms::event::PanicRecordingAction:
            return Qn::EventsActions_StartPanicRecording_Help;
        case vms::event::SendMailAction:
            return Qn::EventsActions_SendMail_Help;
        case vms::event::ShowPopupAction:
            return Qn::EventsActions_ShowNotification_Help;
        case vms::event::PlaySoundOnceAction:
            return Qn::EventsActions_PlaySound_Help;
        case vms::event::PlaySoundAction:
            return Qn::EventsActions_PlaySoundRepeated_Help;
        case vms::event::SayTextAction:
            return Qn::EventsActions_Speech_Help;
        case vms::event::DiagnosticsAction:
            return Qn::EventsActions_Diagnostics_Help;
        case vms::event::ShowOnAlarmLayoutAction:
            return Qn::EventsActions_ShowOnAlarmLayout_Help;
        case vms::event::BookmarkAction:
            return Qn::EventsActions_Bookmark_Help;
        case vms::event::ExecutePtzPresetAction:
            return Qn::EventsActions_ExecutePtzPreset_Help;
        case vms::event::ShowTextOverlayAction:
            return Qn::EventsActions_ShowTextOverlay_Help;
        case vms::event::ExecHttpRequestAction:
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
