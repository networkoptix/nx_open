// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_levels.h"

#include <algorithm>
#include <ranges>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>

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

int QnNotificationLevel::priority(nx::vms::common::SystemContext* systemContext,
    nx::vms::common::system_health::MessageType messageType)
{
    using nx::vms::common::system_health::MessageType;
    // Priority of messages from less important to most important. List is ordered by design
    // team in the specification.
    static const std::vector priorityOrder = {
        // Important priorities.
        MessageType::notificationLanguageDiffers,
        MessageType::saasInShutdownState,
        MessageType::saasInSuspendedState,
        MessageType::saasIntegrationServicesOverused,
        MessageType::saasCloudStorageServicesOverused,
        MessageType::saasLocalRecordingServicesOverused,
        MessageType::metadataStorageNotSet,
        MessageType::replacedDeviceDiscovered,
        MessageType::cameraRecordingScheduleIsInvalid,
        MessageType::metadataOnSystemStorage,
        MessageType::usersEmailIsEmpty,
        MessageType::noInternetForTimeSync,
        MessageType::defaultCameraPasswords,
        MessageType::smtpIsNotSet,
        MessageType::noLicenses,
        MessageType::emailIsEmpty,

        // Critical priorities.
        MessageType::showMissedCallInformer,
        MessageType::remoteArchiveSyncError,
        MessageType::storagesNotConfigured,
        MessageType::backupStoragesNotConfigured,
        MessageType::archiveIntegrityFailed,
        MessageType::emailSendError};

    const int level = static_cast<int>(valueOf(systemContext, messageType)) << 8;
    int priority = 0;
    if (const auto it = std::ranges::find(priorityOrder, messageType); it != priorityOrder.end())
        priority = std::distance(priorityOrder.begin(), it);

    return level | priority;
}

QnNotificationLevel::Value QnNotificationLevel::valueOf(
    nx::vms::common::SystemContext* systemContext,
    nx::vms::common::system_health::MessageType messageType)
{
    using nx::vms::common::system_health::MessageType;

    switch (messageType)
    {
        case MessageType::cloudPromo:
            return QnNotificationLevel::Value::OtherNotification;

        // Gray notifications.
        case MessageType::archiveRebuildCanceled:
            return QnNotificationLevel::Value::CommonNotification;

        // Green notifications.
        case MessageType::archiveRebuildFinished:
        case MessageType::archiveFastScanFinished: //< This one is never displayed though.
        case MessageType::showIntercomInformer:
            return QnNotificationLevel::Value::SuccessNotification;

        // Yellow notifications.
        case MessageType::emailIsEmpty:
        case MessageType::noLicenses:
        case MessageType::smtpIsNotSet:
        case MessageType::defaultCameraPasswords:
        case MessageType::noInternetForTimeSync:
        case MessageType::usersEmailIsEmpty:
        case MessageType::metadataOnSystemStorage:
        case MessageType::cameraRecordingScheduleIsInvalid:
        case MessageType::replacedDeviceDiscovered:
        case MessageType::metadataStorageNotSet:
        case MessageType::saasLocalRecordingServicesOverused:
        case MessageType::saasCloudStorageServicesOverused:
        case MessageType::saasIntegrationServicesOverused:
        case MessageType::saasInSuspendedState:
        case MessageType::saasInShutdownState:
        case MessageType::notificationLanguageDiffers:
            return QnNotificationLevel::Value::ImportantNotification;

        case MessageType::saasTierIssue:
        {
            auto saas = systemContext->saasServiceManager();
            std::optional<int> daysLeft = saas->tierGracePeriodDaysLeft();
            if (daysLeft.has_value() && *daysLeft <= 10)
                return QnNotificationLevel::Value::CriticalNotification;
            return QnNotificationLevel::Value::ImportantNotification;
        }

        // Red notifications.
        case MessageType::emailSendError:
        case MessageType::archiveIntegrityFailed:
        case MessageType::backupStoragesNotConfigured:
        case MessageType::storagesNotConfigured:
        case MessageType::remoteArchiveSyncError:
        case MessageType::showMissedCallInformer:
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
        case Value::CommonNotification:    return colorTheme()->color("green");
        case Value::ImportantNotification: return colorTheme()->color("yellow");
        case Value::CriticalNotification:  return colorTheme()->color("red");
        case Value::SuccessNotification:   return colorTheme()->color("green");
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
        case Value::ImportantNotification: return colorTheme()->color("yellow");
        case Value::CriticalNotification:  return colorTheme()->color("red");
        case Value::SuccessNotification:   return colorTheme()->color("green");
        case Value::OtherNotification:     return Qt::white;
        case Value::CommonNotification:
        default:
            return colorTheme()->color("light4");
    }
}
