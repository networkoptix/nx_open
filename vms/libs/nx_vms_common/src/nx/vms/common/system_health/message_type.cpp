// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_type.h"

namespace nx::vms::common::system_health {

bool isMessageVisible(MessageType message)
{
    switch (message)
    {
        case MessageType::archiveFastScanFinished:
            return false;

        default:
            return true;
    }
}

bool isMessageVisibleInSettings(MessageType message)
{
    // Hidden messages must not be disabled.
    if (!isMessageVisible(message))
        return false;

    switch (message)
    {
        case MessageType::cloudPromo:
        case MessageType::defaultCameraPasswords:

        // TODO: Remove in VMS-7724.
        case MessageType::remoteArchiveSyncError:

        case MessageType::replacedDeviceDiscovered:
            return false;

        default:
            return true;
    }
}

bool isMessageLocked(MessageType message)
{
    switch (message)
    {
        case MessageType::cloudPromo:
        case MessageType::defaultCameraPasswords:
        case MessageType::emailIsEmpty:
        case MessageType::noLicenses:
        case MessageType::smtpIsNotSet:
        case MessageType::usersEmailIsEmpty:
        case MessageType::backupStoragesNotConfigured:
        case MessageType::storagesNotConfigured:
        case MessageType::noInternetForTimeSync:
        case MessageType::cameraRecordingScheduleIsInvalid:
        case MessageType::replacedDeviceDiscovered:
        case MessageType::remoteArchiveSyncError:
        case MessageType::metadataStorageNotSet:
        case MessageType::metadataOnSystemStorage:
            return true;
        default:
            return false;
    }
}

std::set<MessageType> allVisibleMessageTypes()
{
    std::set<MessageType> result;
    for (int i = 0; i < (int) MessageType::count; ++i)
    {
        const auto messageType = static_cast<MessageType>(i);
        if (isMessageVisibleInSettings(messageType))
            result.insert(messageType);
    }
    return result;
}

} // namespace nx::vms::common::system_health
