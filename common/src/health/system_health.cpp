#include "system_health.h"

#include <QObject>


namespace QnSystemHealth {

    QString messageName(MessageType messageType) {
        switch (messageType) {
        case EmailIsEmpty:
            return QObject::tr("Your E-Mail address is not set.");
        case NoLicenses:
            return QObject::tr("You have no licenses.");
        case SmtpIsNotSet:
            return QObject::tr("E-Mail server is not set.");
        case UsersEmailIsEmpty:
            return QObject::tr("Some users have not set their E-Mail addresses.");
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

    QString messageDescription(MessageType messageType) {
        switch (messageType) {
        case EmailIsEmpty: {
                QString result = QObject::tr("Your E-Mail address is not set.");
                result += QLatin1Char('\n');
                result += QObject::tr("You cannot receive system notifications via E-Mail.");
                return result;
            }
        case NoLicenses:
            return QObject::tr("You have no licenses.");
        case SmtpIsNotSet:
            return QObject::tr("E-Mail server is not set.");
        case UsersEmailIsEmpty: {
                QString result = QObject::tr("Some users have not set their E-Mail addresses.");
                result += QLatin1Char('\n');
                result += QObject::tr("They cannot receive system notifications via E-Mail.");
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
