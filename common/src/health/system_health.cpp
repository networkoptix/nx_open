#include "system_health.h"

namespace QnSystemHealth {

    QString toString(MessageType messageType) {
        switch (messageType) {
        case NotDefined:
            return QLatin1String("---");
        case EmailIsEmpty:
            return QObject::tr("Your email is empty");
        default:
            break;
        }
        return QString();
    }

}
