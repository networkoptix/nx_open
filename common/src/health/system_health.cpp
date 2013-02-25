#include "system_health.h"

namespace QnSystemHealth {

    QString toString(MessageType messageType) {
        switch (messageType) {
        case NotDefined:
            return QLatin1String("---");
        case EmailIsEmpty:
            return QObject::tr("Your email is empty.");
        case NoLicenses:
            return QObject::tr("You have no licenses.");
        case SmtpIsNotSet:
            return QObject::tr("Mail server is not set.");
        case UsersEmailIsEmpty:
            return QObject::tr("Some users have not set email.");
        case EmailSendError:
            return QObject::tr("Email could not be sent.");
        default:
            break;
        }
        return QString();
    }

}
