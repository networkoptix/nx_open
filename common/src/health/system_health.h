#pragma once

#include <QtCore/QString>
#include <QtCore/QObject>

#include "business/business_fwd.h"

namespace QnSystemHealth {

// enum order change is forbidden as leads to stored settings failure.
//TODO: #GDM refactor settings storage
enum MessageType
{
    // These messages are generated on the client
    EmailIsEmpty,
    NoLicenses,
    SmtpIsNotSet,
    UsersEmailIsEmpty,  /*< Other user's email is empty. */
    ConnectionLost,     /*< Current user email is empty. */
    NoPrimaryTimeServer,
    SystemIsReadOnly,

    /* These messages are sent from server */
    EmailSendError,
    StoragesNotConfigured,
    StoragesAreFull,
    ArchiveRebuildFinished,
    ArchiveRebuildCanceled,
    ArchiveFastScanFinished,

    CloudPromo, //local promo message

    Count,

    RemoteArchiveSyncStarted,
    RemoteArchiveSyncFinished,
    RemoteArchiveSyncProgress,
    RemoteArchiveSyncError
};

/** Some messages are not to be displayed in any case. */
bool isMessageVisible(MessageType message);

/** Some messages must not be displayed in settings dialog, so user cannot disable them. */
bool isMessageOptional(MessageType message);

/** Some messages must not be auto-hidden by timeout. */
bool isMessageLocked(MessageType message);

}

Q_DECLARE_METATYPE(QnSystemHealth::MessageType);
