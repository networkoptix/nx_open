// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_levels.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>

using namespace nx::vms::client::core;

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

QnNotificationLevel::Value QnNotificationLevel::valueOf(
    nx::vms::common::system_health::MessageType messageType)
{
    using nx::vms::common::system_health::MessageType;

    switch (messageType)
    {
        case MessageType::cloudPromo:
            return QnNotificationLevel::Value::OtherNotification;

        // Gray notifications.
        case MessageType::archiveRebuildCanceled:
        case MessageType::metadataOnSystemStorage:
            return QnNotificationLevel::Value::CommonNotification;

        // Green notifications.
        case MessageType::archiveRebuildFinished:
        case MessageType::archiveFastScanFinished: //< This one is never displayed though.
            return QnNotificationLevel::Value::SuccessNotification;

        // Yellow notifications.
        case MessageType::emailIsEmpty:
        case MessageType::noLicenses:
        case MessageType::smtpIsNotSet:
        case MessageType::defaultCameraPasswords:
        case MessageType::noInternetForTimeSync:
        case MessageType::usersEmailIsEmpty:
        case MessageType::cameraRecordingScheduleIsInvalid:
        case MessageType::replacedDeviceDiscovered:
        case MessageType::metadataStorageNotSet:
        case MessageType::saasLocalRecordingServicesOverused:
        case MessageType::saasCloudStorageServicesOverused:
        case MessageType::saasIntegrationServicesOverused:
        case MessageType::saasInSuspendedState:
        case MessageType::saasInShutdownState:
            return QnNotificationLevel::Value::ImportantNotification;

        // Red notifications.
        case MessageType::emailSendError:
        case MessageType::archiveIntegrityFailed:
        case MessageType::backupStoragesNotConfigured:
        case MessageType::storagesNotConfigured:
        case MessageType::remoteArchiveSyncError:
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
