#include "business_help.h"

#include <ui/help/help_topics.h>

int QnBusiness::eventHelpId(BusinessEventType::Value type) {
    switch (type) {
    case BusinessEventType::Camera_Motion:
        return Qn::EventsActions_CameraMotion_Help;
    case BusinessEventType::Camera_Input:
        return Qn::EventsActions_CameraInput_Help;
    case BusinessEventType::Camera_Disconnect:
        return Qn::EventsActions_CameraDisconnected_Help;
    case BusinessEventType::Storage_Failure:
        return Qn::EventsActions_StorageFailure_Help;
    case BusinessEventType::Network_Issue:
        return Qn::EventsActions_NetworkIssue_Help;
    case BusinessEventType::Camera_Ip_Conflict:
        return Qn::EventsActions_CameraIpConflict_Help;
    case BusinessEventType::MediaServer_Failure:
        return Qn::EventsActions_MediaServerFailure_Help;
    case BusinessEventType::MediaServer_Conflict:
        return Qn::EventsActions_MediaServerConflict_Help;
    case BusinessEventType::MediaServer_Started:
        return Qn::EventsActions_MediaServerStarted_Help;
    default:
        return -1;
    }
}

int QnBusiness::actionHelpId(BusinessActionType::Value type) {
    switch (type) {
    case BusinessActionType::CameraOutput:
    case BusinessActionType::CameraOutputInstant:
        return Qn::EventsActions_CameraOutput_Help;
    case BusinessActionType::CameraRecording:
        return Qn::EventsActions_StartRecording_Help;
    case BusinessActionType::PanicRecording:
        return Qn::EventsActions_StartPanicRecording_Help;
    case BusinessActionType::SendMail:
        return Qn::EventsActions_SendMail_Help;
    case BusinessActionType::ShowPopup:
        return Qn::EventsActions_ShowNotification_Help;
    case BusinessActionType::PlaySound:
        return Qn::EventsActions_PlaySound_Help;
    case BusinessActionType::PlaySoundRepeated:
        return Qn::EventsActions_PlaySoundRepeated_Help;
    case BusinessActionType::SayText:
        return Qn::EventsActions_Speech_Help;
    case BusinessActionType::Diagnostics:
        return Qn::EventsActions_Diagnostics_Help;
    case BusinessActionType::Bookmark:
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
