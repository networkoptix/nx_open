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
            return nx::reflect::enumeration::isValidEnumValue(message);
;
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
        case MessageType::remoteArchiveSyncAvailable:

        // TODO: Remove in VMS-7724.
        case MessageType::remoteArchiveSyncFinished:
        case MessageType::remoteArchiveSyncProgress:
        case MessageType::remoteArchiveSyncError:
        case MessageType::remoteArchiveSyncStopSchedule:
        case MessageType::remoteArchiveSyncStopAutoMode:
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
        case MessageType::saasLocalRecordingServicesOverused:
        case MessageType::saasCloudStorageServicesOverused:
        case MessageType::saasIntegrationServicesOverused:
        case MessageType::saasInSuspendedState:
        case MessageType::saasInShutdownState:

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

std::set<MessageType> defaultMessageTypes()
{
    auto result = allVisibleMessageTypes();
    result.erase(MessageType::emailIsEmpty);
    result.erase(MessageType::usersEmailIsEmpty);
    return result;
}

} // namespace nx::vms::common::system_health
