// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_type.h"

#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/system_context.h>

namespace {

using namespace nx::vms::common::system_health;

static const std::set<MessageType> kMessagesNotSupportedBySaas{
    MessageType::noLicenses
};

static const std::set<MessageType> kMessagesSpecificForSaas{
    MessageType::saasLocalRecordingServicesOverused,
    MessageType::saasCloudStorageServicesOverused,
    MessageType::saasIntegrationServicesOverused,
    MessageType::saasInSuspendedState,
    MessageType::saasInShutdownState,
    MessageType::saasTierIssue,
    MessageType::cloudServiceDisabled,
    MessageType::integrationServiceDisabled,
};

} // namespace

namespace nx::vms::common::system_health {

bool isMessageVisible(MessageType message)
{
    switch (message)
    {
        case MessageType::archiveFastScanFinished:
            return false;

        default:
            return nx::reflect::enumeration::isValidEnumValue(message);
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
        case MessageType::saasTierIssue:
        case MessageType::recordingServiceDisabled:
        case MessageType::cloudServiceDisabled:
        case MessageType::integrationServiceDisabled:
        case MessageType::showIntercomInformer:
        case MessageType::showMissedCallInformer:
        case MessageType::notificationLanguageDiffers:
        case MessageType::cloudStorageIsEnabled:
        case MessageType::cloudStorageIsAvailable:
            return true;

        default:
            return false;
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
        case MessageType::notificationLanguageDiffers:

        // TODO: Remove in VMS-7724.
        case MessageType::remoteArchiveSyncError:
        case MessageType::replacedDeviceDiscovered:
            return false;

        default:
            return true;
    }
}

MessageTypePredicate isMessageApplicableForLicensingMode(SystemContext* systemContext)
{
    const auto isSaasSystem = nx::vms::common::saas::saasInitialized(systemContext);
    return
        [isSaasSystem](const MessageType messageType)
    {
        return isSaasSystem
            ? !kMessagesNotSupportedBySaas.contains(messageType)
            : !kMessagesSpecificForSaas.contains(messageType);
    };
}

bool isMessageIntercom(MessageType message)
{
    return message == MessageType::showIntercomInformer
        || message == MessageType::showMissedCallInformer;
}

std::set<MessageType> allMessageTypes(const MessageTypePredicateList& predicates)
{
    std::set<MessageType> result;
    for (int i = 0; i < (int) MessageType::count; ++i)
    {
        const auto messageType = static_cast<MessageType>(i);
        if (std::all_of(predicates.cbegin(), predicates.cend(),
            [messageType](const auto& predicate){ return predicate(messageType); }))
        {
            result.insert(messageType);
        }
    }
    return result;
}

} // namespace nx::vms::common::system_health
