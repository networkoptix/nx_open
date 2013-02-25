#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <QString>

namespace QnSystemHealth {

    enum MessageType {
        // These messages are generated on the client
        EmailIsEmpty,
        NoLicenses,
        SmtpIsNotSet,
        UsersEmailIsEmpty,

        // These messages are sent from mediaserver
        EmailSendError,
        StoragesNotConfigured,
        StoragesAreFull,

        NotDefined,

        MessageTypeCount = NotDefined
    };

    QString toString(MessageType messageType);
}


#endif // SYSTEM_HEALTH_H
