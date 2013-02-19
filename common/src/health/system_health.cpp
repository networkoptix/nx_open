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
        default:
            break;
        }
        return QString();
    }

}
