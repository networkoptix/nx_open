// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_levels.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;

QnNotificationLevel::Value QnNotificationLevel::convert(nx::vms::event::Level level)
{
    return static_cast<QnNotificationLevel::Value>(level);
}

QnNotificationLevel::Value QnNotificationLevel::valueOf(const nx::vms::event::AbstractActionPtr& action)
{
    return convert(nx::vms::event::levelOf(action));
};

QnNotificationLevel::Value QnNotificationLevel::valueOf(const nx::vms::event::EventParameters& params)
{
    return convert(nx::vms::event::levelOf(params));
}

QnNotificationLevel::Value QnNotificationLevel::valueOf(QnSystemHealth::MessageType messageType)
{
    switch (messageType)
    {
        case QnSystemHealth::CloudPromo:
            return QnNotificationLevel::Value::OtherNotification;

        // Gray notifications.
        case QnSystemHealth::ArchiveRebuildCanceled:
        case QnSystemHealth::RemoteArchiveSyncAvailable:
        case QnSystemHealth::RemoteArchiveSyncProgress:
        case QnSystemHealth::metadataOnSystemStorage:
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
        case QnSystemHealth::DefaultCameraPasswords:
        case QnSystemHealth::NoInternetForTimeSync:
        case QnSystemHealth::UsersEmailIsEmpty:
        case QnSystemHealth::cameraRecordingScheduleIsInvalid:
        case QnSystemHealth::replacedDeviceDiscovered:
        case QnSystemHealth::RemoteArchiveSyncStopSchedule:
        case QnSystemHealth::RemoteArchiveSyncStopAutoMode:
        case QnSystemHealth::metadataStorageNotSet:
            return QnNotificationLevel::Value::ImportantNotification;

        // Red notifications.
        case QnSystemHealth::EmailSendError:
        case QnSystemHealth::ArchiveIntegrityFailed:
        case QnSystemHealth::backupStoragesNotConfigured:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::RemoteArchiveSyncError:
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
        case Value::CommonNotification:    return colorTheme()->color("green_l2");
        case Value::ImportantNotification: return colorTheme()->color("yellow_core");
        case Value::CriticalNotification:  return colorTheme()->color("red_l2");
        case Value::SuccessNotification:   return colorTheme()->color("green_l2");
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
        case Value::ImportantNotification: return colorTheme()->color("yellow_core");
        case Value::CriticalNotification:  return colorTheme()->color("red_l2");
        case Value::SuccessNotification:   return colorTheme()->color("green_l2");
        case Value::OtherNotification:     return Qt::white;
        case Value::CommonNotification:
        default:
            return colorTheme()->color("light10");
    }
}
