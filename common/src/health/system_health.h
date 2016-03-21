#pragma once

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

        /* These messages are sent from server */
        EmailSendError,
        StoragesNotConfigured,
        StoragesAreFull,
        ArchiveRebuildFinished,
        ArchiveRebuildCanceled,
        ArchiveFastScanFinished,

        //NotDefined,

        Count
    };

    bool isMessageVisible(MessageType message);
}

Q_DECLARE_METATYPE(QnSystemHealth::MessageType);
