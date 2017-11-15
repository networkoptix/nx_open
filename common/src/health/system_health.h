#pragma once

#include <QtCore/QString>
#include <QtCore/QObject>

#include <nx/vms/event/event_fwd.h>

namespace QnSystemHealth {

// enum order change is forbidden as leads to stored settings failure.
// TODO: #GDM refactor settings storage
enum MessageType
{
    // These messages are generated on the client
    EmailIsEmpty,       /*< Current user email is empty. */
    NoLicenses,
    SmtpIsNotSet,
    UsersEmailIsEmpty,  /*< Other user's email is empty. */
    NoPrimaryTimeServer, //< Message is not used any more. Leaving here to avoid protocol change.
    SystemIsReadOnly,

    /* These messages are sent from server */
    EmailSendError,
    StoragesNotConfigured,
    StoragesAreFull,
    ArchiveRebuildFinished,
    ArchiveRebuildCanceled,
    ArchiveFastScanFinished,
    ArchiveIntegrityFailed,

    RemoteArchiveSyncStarted,
    RemoteArchiveSyncFinished,
    RemoteArchiveSyncProgress,
    RemoteArchiveSyncError,

    CloudPromo, //local promo message

    Count
};

bool isRemoteArchiveMessage(MessageType);

/** Some messages are not to be displayed in any case. */
bool isMessageVisible(MessageType message);

/** Some messages must not be displayed in settings dialog, so user cannot disable them. */
bool isMessageOptional(MessageType message);

/** Some messages must not be auto-hidden by timeout. */
bool isMessageLocked(MessageType message);

}

Q_DECLARE_METATYPE(QnSystemHealth::MessageType);
