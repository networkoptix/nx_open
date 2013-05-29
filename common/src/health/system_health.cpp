#include "system_health.h"

#include <QObject>


namespace QnSystemHealth {

    QString toString(MessageType messageType, bool shortMode) {
        switch (messageType) {
        case NotDefined:
            return QLatin1String("---");
        case EmailIsEmpty: {
                QString result = QObject::tr("Your E-Mail address\nis not set.");
                if (!shortMode) {
                    result += QLatin1Char('\n');
                    result += QObject::tr("You cannot receive system\n notifications via E-Mail.");
                }
            return result;
            }
        case NoLicenses:
            return QObject::tr("You have no licenses.");
        case SmtpIsNotSet:
            return QObject::tr("E-Mail server is not set.");
        case UsersEmailIsEmpty: {
                QString result = QObject::tr("Some users have not set their E-Mail addresses.");
                if (!shortMode) {
                    result += QLatin1Char('\n');
                    result += QObject::tr("They cannot receive system notifications via E-Mail.");
                }
            return result;
            }
        case ConnectionLost:
            return QObject::tr("Connection to Enterprise Controller is lost.");

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
