// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/vms/event/event_fwd.h>

// TODO: #sivanov refactor settings storage - move to User Settings tab on server.
namespace QnSystemHealth {

// IMPORTANT!!!
// Enum order change is forbidden as leads to stored settings failure and protocol change.
enum MessageType
{
    // These messages are generated on the client.

    EmailIsEmpty = 0,           /**< Current user email is empty. */
    NoLicenses = 1,
    SmtpIsNotSet = 2,
    UsersEmailIsEmpty = 3,      /**< Other user's email is empty. */
    /* ConnectionLost = 4 */
    /* NoPrimaryTimeServer = 5 */
    /* SystemIsReadOnly = 6, */

    // These messages are sent from server.

    EmailSendError = 7,
    StoragesNotConfigured = 8, //< Generated on the client side since 4.1 hotfix.
    /* StoragesAreFull = 9 */
    ArchiveRebuildFinished = 10,
    ArchiveRebuildCanceled = 11,
    ArchiveFastScanFinished = 12,

    CloudPromo = 13, /**< Promo message. Generated on the client side. */

    // RemoteArchiveSyncStarted = 14,
    RemoteArchiveSyncFinished = 15,
    RemoteArchiveSyncProgress = 16,
    RemoteArchiveSyncError = 17,

    ArchiveIntegrityFailed = 18,

    DefaultCameraPasswords = 19, //< Generated on the client side.
    NoInternetForTimeSync = 20, //< Generated on the client side.
    backupStoragesNotConfigured = 21, //< Generated on the client side since 4.3.

    cameraRecordingScheduleIsInvalid = 22, //< Generated on the client side.
    replacedDeviceDiscovered = 23, //< Camera discovered is currently replaced by the given one.

    RemoteArchiveSyncAvailable = 24,

    // IMPORTANT!!!
    // Enum order change is forbidden as leads to stored settings failure and protocol change.

    Count //< Must not be greater than 64 due to storage limitation (not accounting skipped flags).
};

/** Some messages are not to be displayed in any case. */
NX_VMS_COMMON_API bool isMessageVisible(MessageType message);

/** Some messages must not be displayed in settings dialog, so user cannot disable them. */
NX_VMS_COMMON_API bool isMessageVisibleInSettings(MessageType message);

/** Some messages must not be auto-hidden by timeout. */
NX_VMS_COMMON_API bool isMessageLocked(MessageType message);

NX_VMS_COMMON_API QList<MessageType> allVisibleMessageTypes();

NX_VMS_COMMON_API QSet<MessageType> unpackVisibleInSettings(quint64 packed);

/**
 * Store visible messages to settings. Original value is required to mantain client versions
 * compatibility (until storage moved to server).
 */
NX_VMS_COMMON_API quint64 packVisibleInSettings(quint64 base, QSet<MessageType> messageTypes);

} // namespace QnSystemHealth

Q_DECLARE_METATYPE(QnSystemHealth::MessageType)

