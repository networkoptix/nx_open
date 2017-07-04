#include "system_health_helper.h"

#include <nx/vms/event/actions/abstract_action.h>

#include <nx/network/app_info.h>
#include <nx/utils/string.h>

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
            return tr("Some users have not set their Email addresses");
        case QnSystemHealth::NoPrimaryTimeServer:
            return tr("Select server for others to synchronize time with");
        case QnSystemHealth::SystemIsReadOnly:
            return tr("System is in safe mode");
        case QnSystemHealth::EmailSendError:
            return tr("Error while sending Email");
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
    //TODO: #GDM #3.1 elide on the widget level
    static const int kMaxNameLength = 30;

    switch (messageType)
    {
        case QnSystemHealth::UsersEmailIsEmpty:
            return tr("Email address is not set for user %1")
                .arg(nx::utils::elideString(resourceName, kMaxNameLength));

        case QnSystemHealth::CloudPromo:
        {
            const QString kLearnMoreText = tr("Learn more");
            const QString kCloudBeta = lit("<b>%1</b> Beta").arg(nx::network::AppInfo::cloudName());

            const QString kMessage = tr("Check out %1 &mdash; connect to your servers from anywhere",
                "%1 is the cloud name (like 'Nx Cloud')").arg(kCloudBeta);

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
            messageParts << tr("Email address is not set.") << tr("You cannot receive System notifications by Email.");
            break;
        case QnSystemHealth::SmtpIsNotSet:
            messageParts << tr("Email server is not set.") << tr("You cannot receive System notifications by Email.");
            break;
        case QnSystemHealth::UsersEmailIsEmpty:
            messageParts << tr("Some users have not set their Email addresses.") << tr("They cannot receive System notifications by Email.");
            break;
        case QnSystemHealth::NoPrimaryTimeServer:
            messageParts << tr("Server times are not synchronized and a common time could not be detected automatically.");
            break;
        case QnSystemHealth::SystemIsReadOnly:
            messageParts << tr("System is running in safe mode.") << tr("Any configuration changes except license activation are impossible.");
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
