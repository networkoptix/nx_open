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
        default:
            break;
        }
        return QString();
    }

}
