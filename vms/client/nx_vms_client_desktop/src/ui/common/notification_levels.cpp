#include "notification_levels.h"

#include <utils/common/warnings.h>

#include <ui/style/globals.h>

using namespace nx;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

QnNotificationLevel::Value QnNotificationLevel::valueOf(const nx::vms::event::AbstractActionPtr& action)
{
    return static_cast<QnNotificationLevel::Value>(nx::vms::event::levelOf(action));
};

QnNotificationLevel::Value QnNotificationLevel::valueOf(const nx::vms::event::EventParameters& params)
{
    return static_cast<QnNotificationLevel::Value>(nx::vms::event::levelOf(params));
}

QnNotificationLevel::Value QnNotificationLevel::valueOf(QnSystemHealth::MessageType messageType)
{
    switch (messageType)
    {
        case QnSystemHealth::CloudPromo:
            return QnNotificationLevel::Value::OtherNotification;

        // Gray notifications.
        case QnSystemHealth::ArchiveRebuildCanceled:
        case QnSystemHealth::RemoteArchiveSyncStarted:
        case QnSystemHealth::RemoteArchiveSyncProgress:
            return QnNotificationLevel::Value::CommonNotification;

        // Green notifications.
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::RemoteArchiveSyncFinished:
        case QnSystemHealth::ArchiveFastScanFinished: //< This one is never displayed though.
            return QnNotificationLevel::Value::SuccessNotification;

        // Yellow notifications.
        case QnSystemHealth::EmailIsEmpty:
        case QnSystemHealth::NoLicenses:
        case QnSystemHealth::SmtpIsNotSet:
        case QnSystemHealth::SystemIsReadOnly:
        case QnSystemHealth::RemoteArchiveSyncError:
        case QnSystemHealth::DefaultCameraPasswords:
        case QnSystemHealth::NoInternetForTimeSync:
        case QnSystemHealth::UsersEmailIsEmpty:
            return QnNotificationLevel::Value::ImportantNotification;

        // Red notifications.
        case QnSystemHealth::EmailSendError:
        case QnSystemHealth::ArchiveIntegrityFailed:
        case QnSystemHealth::StoragesNotConfigured:
            return QnNotificationLevel::Value::CriticalNotification;

        default:
            NX_ASSERT(false, "All cases must be handled here");
            break;
    }
    return QnNotificationLevel::Value::CriticalNotification;
}

QColor QnNotificationLevel::notificationColor(Value level)
{
    switch (level)
    {
        case Value::NoNotification:        return Qt::transparent;
        case Value::OtherNotification:     return Qt::white;
        case Value::CommonNotification:    return qnGlobals->notificationColorCommon();
        case Value::ImportantNotification: return qnGlobals->notificationColorImportant();
        case Value::CriticalNotification:  return qnGlobals->notificationColorCritical();
        case Value::SuccessNotification:   return qnGlobals->notificationColorCommon();
        default:
            NX_ASSERT(false, "All enum values must be handled");
            break;
    }
    return QColor();
}

QColor QnNotificationLevel::notificationTextColor(Value level)
{
    switch (level)
    {
        case Value::ImportantNotification: return qnGlobals->warningTextColor();
        case Value::CriticalNotification:  return qnGlobals->errorTextColor();
        case Value::SuccessNotification:   return qnGlobals->successTextColor();
        case Value::OtherNotification:     return Qt::white;
        default: return QColor(); //< Undefined and should be treated as default.
    }
}
