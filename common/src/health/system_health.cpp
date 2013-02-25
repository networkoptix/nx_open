#include "system_health.h"

namespace QnSystemHealth {

    QString toString(MessageType messageType) {
        switch (messageType) {
        case NotDefined:
            return QLatin1String("---");
        case EmailIsEmpty:
            return QObject::tr("Your E-Mail address is not set.\n"\
                               "You cannot receive system notifications via E-Mail");
        case NoLicenses:
            return QObject::tr("You have no licenses.");
        case SmtpIsNotSet:
            return QObject::tr("E-Mail server is not set.");
        case UsersEmailIsEmpty:
            return QObject::tr("Some users have not set their E-Mail addresses.\n"\
                               "They cannot receive system notifications via E-Mail");
        case EmailSendError:
            return QObject::tr("Error while sending E-Mail.");
        case StoragesAreFull:
            return QObject::tr("Some storages are full.");
        case StoragesNotConfigured:
            return QObject::tr("Storages are not configured.");

        default:
            break;
        }
        return QString();
    }

}
