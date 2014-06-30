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
    default:
        return -1;
    }
}

int QnBusiness::actionHelpId(QnBusiness::ActionType type) {
    switch (type) {
    case CameraOutputAction:
    case CameraOutputOnceAction:
        return Qn::EventsActions_CameraOutput_Help;
    case CameraRecordingAction:
        return Qn::EventsActions_StartRecording_Help;
    case PanicRecordingAction:
        return Qn::EventsActions_StartPanicRecording_Help;
    case SendMailAction:
        return Qn::EventsActions_SendMail_Help;
    case ShowPopupAction:
        return Qn::EventsActions_ShowNotification_Help;
    case PlaySoundOnceAction:
        return Qn::EventsActions_PlaySound_Help;
    case PlaySoundAction:
        return Qn::EventsActions_PlaySoundRepeated_Help;
    case SayTextAction:
        return Qn::EventsActions_Speech_Help;
    case DiagnosticsAction:
        return Qn::EventsActions_Diagnostics_Help;
    case BookmarkAction:
        return -1;
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
    case QnSystemHealth::ConnectionLost:
        return Qn::EventsActions_EcConnectionLost_Help;
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
