#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <QtCore/QString>
#include <QtCore/QObject>
#include "business/business_fwd.h"

namespace QnSystemHealth {
    enum MessageType {
        // These messages are generated on the client
        EmailIsEmpty,
        NoLicenses,
        SmtpIsNotSet,
        UsersEmailIsEmpty,
        ConnectionLost,
        NoPrimaryTimeServer,
        SystemIsReadOnly,

        // These messages are sent from server
        EmailSendError,
        StoragesNotConfigured,
        StoragesAreFull,
        ArchiveRebuildFinished,
        ArchiveRebuildCanceled,

        NotDefined,

        MessageTypeCount = ArchiveRebuildFinished
    };
}

#endif // SYSTEM_HEALTH_H
