#include "system_health.h"
#include "business/actions/abstract_business_action.h"
#include "utils/common/synctime.h"


QString QnSystemHealthStringsHelper::messageTitle(QnSystemHealth::MessageType messageType) {
    switch (messageType) {
    case QnSystemHealth::EmailIsEmpty:
        return tr("Email address is not set");
    case QnSystemHealth::NoLicenses:
        return tr("No licenses");
    case QnSystemHealth::SmtpIsNotSet:
        return tr("Email server is not set");
    case QnSystemHealth::UsersEmailIsEmpty:
        return tr("Some users have not set their email addresses");
    case QnSystemHealth::ConnectionLost:
        return tr("Connection to server lost");
    case QnSystemHealth::NoPrimaryTimeServer:
        return tr("Select server for others to synchronize time with");
    case QnSystemHealth::SystemIsReadOnly:
        return tr("System is in safe mode");
    case QnSystemHealth::EmailSendError:
        return tr("Error while sending email");
    case QnSystemHealth::StoragesAreFull:
        return tr("Storage is full");
    case QnSystemHealth::StoragesNotConfigured:
        return tr("Storage is not configured");
    case QnSystemHealth::ArchiveRebuildFinished:
        return tr("Rebuilding archive index is completed");
    case QnSystemHealth::ArchiveRebuildCanceled:
        return tr("Rebuilding archive index is canceled by user");
    case QnSystemHealth::ArchiveBackupFinished:
        return tr("Archive backup finished");
    default:
        break;
    }
    return QString();
}


QString QnSystemHealthStringsHelper::messageName(QnSystemHealth::MessageType messageType, QString resourceName) {
    switch (messageType) {
    case QnSystemHealth::UsersEmailIsEmpty:
        return tr("Email address is not set for user %1").arg(resourceName);
    default:
        break;
    }
    return messageTitle(messageType);
}

QString QnSystemHealthStringsHelper::backupPositionToStr(const QDateTime& dt)
{
    QDateTime now = qnSyncTime->currentDateTime();
    int days = dt.daysTo(now);
    int weeks = days / 7;
    days -= weeks *7;
    QString diffStr;
    if (weeks > 0)
        diffStr = tr("%n weeks ", "", weeks);
    if (days > 0)
        diffStr += tr("%n days ", "", days);
    if (!diffStr.isEmpty())
        diffStr = tr("(%1before now)").arg(diffStr);

    return tr("%1 %2 %3").arg(dt.date().toString()).arg(dt.time().toString(lit("hh:mm"))).arg(diffStr);
}

QString QnSystemHealthStringsHelper::messageDescription(QnSystemHealth::MessageType messageType, const QnAbstractBusinessActionPtr &businessAction, QString resourceName) 
{
    QStringList messageParts;

    switch (messageType) {
    case QnSystemHealth::EmailIsEmpty:
        messageParts << tr("Email address is not set.") << tr("You cannot receive system notifications via email.");
        break;
    case QnSystemHealth::SmtpIsNotSet:
        messageParts << tr("Email server is not set.") << tr("You cannot receive system notifications via email.");
        break;
    case QnSystemHealth::UsersEmailIsEmpty:
        messageParts << tr("Some users have not set their email addresses.") << tr("They cannot receive system notifications via email.");
        break;
    case QnSystemHealth::NoPrimaryTimeServer:
        messageParts << tr("Server times are not synchronized and a common time could not be detected automatically." );
        break;
    case QnSystemHealth::SystemIsReadOnly:         
        messageParts << tr("The system is running in safe mode.") << tr("Any configuration changes except license activation are impossible.");
        break;
    case QnSystemHealth::StoragesAreFull:
        messageParts << tr("Storages are full on the following Server:") << resourceName;
        break;
    case QnSystemHealth::StoragesNotConfigured:
        messageParts << tr("Storages are not configured on the following Server:") << resourceName;
        break;
    case QnSystemHealth::NoLicenses:
        messageParts << tr("You have no licenses.") << tr("You cannot record video from cameras.");
        break;
    case QnSystemHealth::ArchiveRebuildFinished:
        messageParts << tr("Rebuilding archive index is completed on the following Server:") << resourceName;
        break;
    case QnSystemHealth::ArchiveRebuildCanceled:
        messageParts << tr("Rebuilding archive index is canceled by user on the following Server:") << resourceName;
        break;
    case QnSystemHealth::ArchiveBackupFinished:
        if (businessAction) {
            qint64 backupPos = businessAction->getRuntimeParams().caption.toLongLong();
            if (backupPos > 0) {
                QDateTime dt = QDateTime::fromMSecsSinceEpoch(backupPos);
                messageParts << tr("Archive backup finished to position %1").arg(backupPositionToStr(dt));
            }
            else {
                messageParts << tr("Nothing to backup (no video archive or no backup cameras configured)");
            }
        }
        else
            messageParts << tr("Archive backup finished");
        break;
    default:
        break;
    }
    if (!messageParts.isEmpty())
        return messageParts.join(L'\n');

    /* Description is ended with a dot, title is not. */
    return messageName(messageType, resourceName) + L'.';
}


