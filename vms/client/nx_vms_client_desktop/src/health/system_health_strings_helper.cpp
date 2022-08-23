// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_strings_helper.h"

#include <nx/branding.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/event/actions/abstract_action.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

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
        case QnSystemHealth::EmailSendError:
            return tr("Error while sending email");
        case QnSystemHealth::StoragesNotConfigured:
            return tr("Storage is not configured");
        case QnSystemHealth::backupStoragesNotConfigured:
            return tr("Backup storage is not configured");
        case QnSystemHealth::ArchiveRebuildFinished:
            return tr("Rebuilding archive index is completed");
        case QnSystemHealth::ArchiveRebuildCanceled:
            return tr("Rebuilding archive index is canceled by user");
        case QnSystemHealth::ArchiveIntegrityFailed:
            return tr("Archive integrity problem detected");
        case QnSystemHealth::NoInternetForTimeSync:
            return tr("The System has no internet access for time synchronization");
        case QnSystemHealth::cameraRecordingScheduleIsInvalid:
            return tr("Camera recording schedule is invalid");
        default:
            break;
    }
    NX_ASSERT(false, "Requesting name for non-visual action");
    return QString();
}


QString QnSystemHealthStringsHelper::messageText(QnSystemHealth::MessageType messageType,
   const QString& resourceName)
{
    namespace html = nx::vms::common::html;

    // TODO: #sivanov Elide on the widget level.
    static const int kMaxNameLength = 30;

    switch (messageType)
    {
        case QnSystemHealth::UsersEmailIsEmpty:
            return tr("Email address is not set for user %1")
                .arg(nx::utils::elideString(resourceName, kMaxNameLength));

        case QnSystemHealth::CloudPromo:
        {
            const bool isLoggedIntoCloud =
                qnCloudStatusWatcher->status() != core::CloudStatusWatcher::LoggedOut;

            const QString kCloudNameText = html::bold(nx::branding::cloudName());
            const QString kMessage = isLoggedIntoCloud
                ? tr("Connect your System to %1 &mdash; make it accessible from anywhere!",
                    "%1 is the cloud name (like Nx Cloud)").arg(kCloudNameText)
                : tr("Check out %1 &mdash; connect to your System from anywhere!",
                    "%1 is the cloud name (like Nx Cloud)").arg(kCloudNameText);

            using nx::vms::utils::SystemUri;
            core::CloudUrlHelper urlHelper(
                SystemUri::ReferralSource::DesktopClient,
                SystemUri::ReferralContext::None);

            static const QString kTemplate = QString::fromLatin1(
                "<style>td {padding-top: %4px; padding-right: %4px}</style>"
                "<p>%1</p>"
                "<table cellpadding='0' cellspacing='0'>"
                "<tr><td>%2</td><td>%3</td></tr>"
                "</table>");

            return kTemplate.arg(kMessage)
                .arg(html::link(tr("Learn more"), urlHelper.aboutUrl()))
                .arg(html::customLink(tr("Connect"), "settings")) //< TODO: Replace to `#settings`.
                .arg(nx::style::Metrics::kStandardPadding);
        }

        case QnSystemHealth::DefaultCameraPasswords:
            return tr("Some cameras require passwords to be set");

        case QnSystemHealth::NoInternetForTimeSync:
            return tr("No server has internet access for time synchronization");

        case QnSystemHealth::RemoteArchiveSyncAvailable:
            return tr("Remote archive synchronization available");
        case QnSystemHealth::RemoteArchiveSyncFinished:
            return tr("Remote archive synchronization has been finished");
        case QnSystemHealth::RemoteArchiveSyncProgress:
            return tr("Remote archive synchronization is in progress");
        case QnSystemHealth::RemoteArchiveSyncError:
            return tr("Error occurred during remote archive synchronization");

        default:
            break;
    }
    return messageTitle(messageType);
}

QString QnSystemHealthStringsHelper::messageTooltip(QnSystemHealth::MessageType messageType,
    QString resourceName)
{
    QStringList messageParts;

    switch (messageType)
    {
        // disable tooltip for promo
        case QnSystemHealth::CloudPromo:
            return QString();

        case QnSystemHealth::DefaultCameraPasswords:
            return QString(); //< TODO: #vkutin #sivanov Some tooltip would be nice.

        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::backupStoragesNotConfigured:
            return QString(); //< Server list is displayed separately.

        case QnSystemHealth::NoInternetForTimeSync:
            return tr("No online server in the System has internet access for time synchronization.");

        case QnSystemHealth::EmailIsEmpty:
            messageParts << tr("Email address is not set.") << tr("You cannot receive System notifications by email.");
            break;
        case QnSystemHealth::SmtpIsNotSet:
            messageParts << tr("Email server is not set.") << tr("You cannot receive System notifications by email.");
            break;
        case QnSystemHealth::UsersEmailIsEmpty:
            messageParts << tr("Some users have not set their email addresses.") << tr("They cannot receive System notifications by email.");
            break;
        case QnSystemHealth::ArchiveIntegrityFailed:
            messageParts << resourceName;
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
        case QnSystemHealth::RemoteArchiveSyncFinished:
            messageParts << tr("Remote archive synchronization has been finished for the following device:") << resourceName;
            break;
        default:
            break;
    }

    if (!messageParts.isEmpty())
        return messageParts.join('\n');

    /* Description is ended with a dot, title is not. */
    return messageText(messageType, resourceName) + '.';
}
