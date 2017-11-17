#pragma once

#include <QtCore/QMetaType>

#include <nx/vms/event/event_fwd.h>

// TODO: #GDM refactor settings storage - move to User Settings tab on server.
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
    SystemIsReadOnly = 5,
    CloudPromo = 12,            /**< Promo message. */

    // These messages are sent from server.

    EmailSendError = 6,
    StoragesNotConfigured = 7,
    StoragesAreFull = 8,
    ArchiveRebuildFinished = 9,
    ArchiveRebuildCanceled = 10,
    ArchiveFastScanFinished = 11,

    RemoteArchiveSyncStarted = 13,
    RemoteArchiveSyncFinished = 14,
    RemoteArchiveSyncProgress = 15,
    RemoteArchiveSyncError = 16,
	
	ArchiveIntegrityFailed = 17,

    // IMPORTANT!!!
    // Enum order change is forbidden as leads to stored settings failure and protocol change.

    Count //< Must not be greater than 64 due to storage limitation
};

bool isRemoteArchiveMessage(MessageType);

/** Some messages are not to be displayed in any case. */
bool isMessageVisible(MessageType message);

/** Some messages must not be displayed in settings dialog, so user cannot disable them. */
bool isMessageVisibleInSettings(MessageType message);

/** Some messages must not be auto-hidden by timeout. */
bool isMessageLocked(MessageType message);

QSet<MessageType> allVisibleMessageTypes();

QSet<MessageType> unpackVisibleInSettings(quint64 packed);

/**
 * Store visible messages to settings. Original value is required to mantain client versions
 * compatibility (until storage moved to server).
 */
quint64 packVisibleInSettings(quint64 base, QSet<MessageType> messageTypes);

} // namespace QnSystemHealth

Q_DECLARE_METATYPE(QnSystemHealth::MessageType)

