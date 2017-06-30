#include "business_help.h"

#include <ui/help/help_topics.h>

int QnBusiness::eventHelpId(QnBusiness::EventType type) {
    switch (type) {
    case CameraMotionEvent:
        return Qn::EventsActions_CameraMotion_Help;
    case CameraInputEvent:
        return Qn::EventsActions_CameraInput_Help;
    case CameraDisconnectEvent:
        return Qn::EventsActions_CameraDisconnected_Help;
    case StorageFailureEvent:
        return Qn::EventsActions_StorageFailure_Help;
    case NetworkIssueEvent:
        return Qn::EventsActions_NetworkIssue_Help;
    case CameraIpConflictEvent:
        return Qn::EventsActions_CameraIpConflict_Help;
    case ServerFailureEvent:
        return Qn::EventsActions_MediaServerFailure_Help;
    case ServerConflictEvent:
        return Qn::EventsActions_MediaServerConflict_Help;
    case ServerStartEvent:
        return Qn::EventsActions_MediaServerStarted_Help;
    case LicenseIssueEvent:
        return Qn::EventsActions_LicenseIssue_Help;
    case BackupFinishedEvent:
        return Qn::EventsActions_BackupFinished_Help;
    default:
        if (type >= UserDefinedEvent)
            return Qn::EventsActions_Generic_Help;

        return -1;
    }
}

int QnBusiness::actionHelpId(QnBusiness::ActionType type) {
    switch (type) {
    case QnBusiness::CameraOutputAction:
        return Qn::EventsActions_CameraOutput_Help;
    case QnBusiness::CameraRecordingAction:
        return Qn::EventsActions_StartRecording_Help;
    case QnBusiness::PanicRecordingAction:
        return Qn::EventsActions_StartPanicRecording_Help;
    case QnBusiness::SendMailAction:
        return Qn::EventsActions_SendMail_Help;
    case QnBusiness::ShowPopupAction:
        return Qn::EventsActions_ShowNotification_Help;
    case QnBusiness::PlaySoundOnceAction:
        return Qn::EventsActions_PlaySound_Help;
    case QnBusiness::PlaySoundAction:
        return Qn::EventsActions_PlaySoundRepeated_Help;
    case QnBusiness::SayTextAction:
        return Qn::EventsActions_Speech_Help;
    case QnBusiness::DiagnosticsAction:
        return Qn::EventsActions_Diagnostics_Help;
    case QnBusiness::ShowOnAlarmLayoutAction:
        return Qn::EventsActions_ShowOnAlarmLayout_Help;
    case QnBusiness::BookmarkAction:
        return Qn::EventsActions_Bookmark_Help;
    case QnBusiness::ExecutePtzPresetAction:
        return Qn::EventsActions_ExecutePtzPreset_Help;
    case QnBusiness::ShowTextOverlayAction:
        return Qn::EventsActions_ShowTextOverlay_Help;
    case QnBusiness::ExecHttpRequestAction:
        return Qn::EventsActions_ExecHttpRequest_Help;
    default:
        return -1;
    }
}

int QnBusiness::healthHelpId(QnSystemHealth::MessageType type) {
    switch(type) {
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
