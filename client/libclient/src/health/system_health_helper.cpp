#include "system_health_helper.h"

#include "business/actions/abstract_business_action.h"

#include <utils/common/app_info.h>

QString QnSystemHealthStringsHelper::messageTitle(QnSystemHealth::MessageType messageType)
{
    switch (messageType)
    {
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
        default:
            break;
    }
    NX_ASSERT(false, Q_FUNC_INFO, "Requesting name for non-visual action");
    return QString();
}


QString QnSystemHealthStringsHelper::messageText(QnSystemHealth::MessageType messageType,
    const QString& resourceName)
{
    switch (messageType)
    {
        case QnSystemHealth::UsersEmailIsEmpty:
            return tr("Email address is not set for user %1").arg(resourceName);
        case QnSystemHealth::CloudPromo:
        {
            const QString kLearnMoreText = tr("Learn more");
            const QString kMessage = tr("Check out <b>%1</b> Beta "
                                        "&mdash; connect to your servers from anywhere",
                "%1 is the cloud name (like 'Nx Cloud')").arg(QnAppInfo::cloudName());

            const QString kTemplate = lit("<p>%1</p><p><a href=\"settings\">%2</a></p>");
            return kTemplate.arg(kMessage, kLearnMoreText);
        }
        default:
            break;
    }
    return messageTitle(messageType);
}

QString QnSystemHealthStringsHelper::messageTooltip(QnSystemHealth::MessageType messageType, QString resourceName)
{
    QStringList messageParts;

    switch (messageType)
    {
        // disable tooltip for promo
        case QnSystemHealth::CloudPromo:
            return QString();

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
            messageParts << tr("Server times are not synchronized and a common time could not be detected automatically.");
            break;
        case QnSystemHealth::SystemIsReadOnly:
            messageParts << tr("The system is running in safe mode.") << tr("Any configuration changes except license activation are impossible.");
            break;
        case QnSystemHealth::StoragesAreFull:
            messageParts << tr("Storage is full on the following Server:") << resourceName;
            break;
        case QnSystemHealth::StoragesNotConfigured:
            messageParts << tr("Storage is not configured on the following Server:") << resourceName;
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
        default:
            break;
    }
    if (!messageParts.isEmpty())
        return messageParts.join(L'\n');

    /* Description is ended with a dot, title is not. */
    return messageText(messageType, resourceName) + L'.';
}
