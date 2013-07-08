#include "system_health.h"

QString QnSystemHealthStringsHelper::messageTitle(QnSystemHealth::MessageType messageType) {
    switch (messageType) {
    case QnSystemHealth::EmailIsEmpty:
        return tr("Your E-Mail address is not set.");
    case QnSystemHealth::NoLicenses:
        return tr("You have no licenses.");
    case QnSystemHealth::SmtpIsNotSet:
        return tr("E-Mail server is not set.");
    case QnSystemHealth::UsersEmailIsEmpty:
        return tr("Some users have not set their E-Mail addresses.");
    case QnSystemHealth::ConnectionLost:
        return tr("Connection to Enterprise Controller is lost.");
    case QnSystemHealth::EmailSendError:
        return tr("Error while sending E-Mail.");
    case QnSystemHealth::StoragesAreFull:
        return tr("Storages are full.");
    case QnSystemHealth::StoragesNotConfigured:
        return tr("Storages are not configured.");
    default:
        break;
    }
    return QString();
}


QString QnSystemHealthStringsHelper::messageName(QnSystemHealth::MessageType messageType, QString resourceName) {
    switch (messageType) {
    case QnSystemHealth::NoLicenses:
        return tr("You have no licenses. You cannot record cameras.");
    case QnSystemHealth::UsersEmailIsEmpty:
        return tr("E-Mail address is not set for user %1.").arg(resourceName);
    default:
        break;
    }
    return messageTitle(messageType);
}

QString QnSystemHealthStringsHelper::messageDescription(QnSystemHealth::MessageType messageType, QString resourceName) {
    switch (messageType) {
    case QnSystemHealth::EmailIsEmpty: {
            QString result = tr("Your E-Mail address is not set.");
            result += QLatin1Char('\n');
            result += tr("You cannot receive system notifications via E-Mail.");
            return result;
        }
    case QnSystemHealth::UsersEmailIsEmpty: {
            QString result = tr("Some users have not set their E-Mail addresses.");
            result += QLatin1Char('\n');
            result += tr("They cannot receive system notifications via E-Mail.");
            return result;
        }
    case QnSystemHealth::StoragesAreFull:
        return tr("Storages are full on the following Media Server: %1.").arg(resourceName);
    case QnSystemHealth::StoragesNotConfigured:
        return tr("Storages are not configured on the following Media Server: %1.").arg(resourceName);

    default:
        break;
    }
    return messageName(messageType, resourceName);
}


