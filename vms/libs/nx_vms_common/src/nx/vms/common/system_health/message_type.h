// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/reflect/enum_instrument.h>
#include <nx/vms/event/event_fwd.h>

// TODO: #sivanov refactor settings storage - move to User Settings tab on server.
namespace nx::vms::common::system_health {

// IMPORTANT!!!
// Enum order change is forbidden as leads to stored settings failure and protocol change.
NX_REFLECTION_ENUM_CLASS(MessageType,
    // These messages are generated on the client.

    emailIsEmpty = 0, //< Current user email is empty.
    noLicenses = 1,
    smtpIsNotSet = 2,
    usersEmailIsEmpty = 3, //< Other user's email is empty.

    // These messages are sent from server.

    emailSendError = 7,
    storagesNotConfigured = 8, //< Generated on the client side since 4.1 hotfix.
    archiveRebuildFinished = 10,
    archiveRebuildCanceled = 11,
    archiveFastScanFinished = 12,

    cloudPromo = 13, //< Promo message. Generated on the client side.

    // TODO: Remove these in VMS-7724.
    remoteArchiveSyncFinished = 15,
    remoteArchiveSyncProgress = 16,
    remoteArchiveSyncError = 17,

    archiveIntegrityFailed = 18,

    defaultCameraPasswords = 19, //< Generated on the client side.
    noInternetForTimeSync = 20, //< Generated on the client side.
    backupStoragesNotConfigured = 21, //< Generated on the client side since 4.3.

    cameraRecordingScheduleIsInvalid = 22, //< Generated on the client side.
    replacedDeviceDiscovered = 23, //< Camera discovered is currently replaced by the given one.

    remoteArchiveSyncAvailable = 24,
    remoteArchiveSyncStopSchedule = 25,
    remoteArchiveSyncStopAutoMode = 26,

    metadataStorageNotSet = 27, //< Generated on the client side.
    metadataOnSystemStorage = 28, //< Generated on the client side.

    saasLocalRecordingServicesOverused = 29, //< Generated on the client side.
    saasCloudStorageServicesOverused = 30, //< Generated on the client side.
    saasIntegrationServicesOverused = 31, //< Generated on the client side.

    saasInSuspendedState = 32, //< Generated on the client side.
    saasInShutdownState = 33, //< Generated on the client side.

    // IMPORTANT!!!
    // Enum order change is forbidden as leads to stored settings failure and protocol change.

    count
)

/** Some messages are not to be displayed in any case. */
NX_VMS_COMMON_API bool isMessageVisible(MessageType message);

/** Some messages must not be displayed in settings dialog, so user cannot disable them. */
NX_VMS_COMMON_API bool isMessageVisibleInSettings(MessageType message);

/** Some messages must not be auto-hidden by timeout. */
NX_VMS_COMMON_API bool isMessageLocked(MessageType message);

NX_VMS_COMMON_API std::set<MessageType> allVisibleMessageTypes();
NX_VMS_COMMON_API std::set<MessageType> defaultMessageTypes();

} // namespace nx::vms::common::system_health
