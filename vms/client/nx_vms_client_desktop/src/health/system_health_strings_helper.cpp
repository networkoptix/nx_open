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

QString QnSystemHealthStringsHelper::messageTitle(MessageType messageType)
{
    switch (messageType)
    {
        case MessageType::emailIsEmpty:
            return tr("Email address is not set");
        case MessageType::noLicenses:
            return tr("No licenses");
        case MessageType::smtpIsNotSet:
            return tr("Email server is not set");
        case MessageType::usersEmailIsEmpty:
            return tr("Some users have not set their email addresses");
        case MessageType::emailSendError:
            return tr("Error while sending email");
        case MessageType::storagesNotConfigured:
            return tr("Storage is not configured");
        case MessageType::backupStoragesNotConfigured:
            return tr("Backup storage is not configured");
        case MessageType::archiveRebuildFinished:
            return tr("Rebuilding archive index is completed");
        case MessageType::archiveRebuildCanceled:
            return tr("Rebuilding archive index is canceled by user");
        case MessageType::archiveIntegrityFailed:
            return tr("Archive integrity problem detected");
        case MessageType::noInternetForTimeSync:
            return tr("The System has no internet access for time synchronization");
        case MessageType::cameraRecordingScheduleIsInvalid:
            return tr("Camera recording schedule is invalid");
        case MessageType::metadataStorageNotSet:
            return tr("Storage for analytics data is not set");
        case MessageType::metadataOnSystemStorage:
            return tr("System storage is used for analytics data");
        case MessageType::saasLocalRecordingServicesOverused:
            return tr("Local recording services overused");
        case MessageType::saasCloudStorageServicesOverused:
            return tr("Cloud storage services overused");
        case MessageType::saasIntegrationServicesOverused:
            return tr("Paid integrations services overused");
        case MessageType::saasInSuspendedState:
            return tr("System suspended");
        case MessageType::saasInShutdownState:
            return tr("System shut down");

        default:
            break;
    }
    NX_ASSERT(false, "Requesting name for non-visual action: %1", messageType);
    return QString();
}

QString QnSystemHealthStringsHelper::messageText(MessageType messageType,
   const QString& resourceName)
{
    namespace html = nx::vms::common::html;

    // TODO: #sivanov Elide on the widget level.
    static const int kMaxNameLength = 30;

    switch (messageType)
    {
        case MessageType::usersEmailIsEmpty:
            return tr("Email address is not set for user %1")
                .arg(nx::utils::elideString(resourceName, kMaxNameLength));

        case MessageType::cloudPromo:
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

        case MessageType::defaultCameraPasswords:
            return tr("Some cameras require passwords to be set");

        case MessageType::noInternetForTimeSync:
            return tr("No server has internet access for time synchronization");

        case MessageType::remoteArchiveSyncError:
            return tr("Error occurred during remote archive synchronization");

        default:
            break;
    }
    return messageTitle(messageType);
}

QString QnSystemHealthStringsHelper::messageTooltip(MessageType messageType,
    QString resourceName)
{
    QStringList messageParts;

    switch (messageType)
    {
        // disable tooltip for promo
        case MessageType::cloudPromo:
            return QString();

        case MessageType::defaultCameraPasswords:
            return QString(); //< TODO: #vkutin #sivanov Some tooltip would be nice.

        case MessageType::storagesNotConfigured:
        case MessageType::backupStoragesNotConfigured:
        case MessageType::metadataStorageNotSet:
            return QString(); //< Server list is displayed separately.

        case MessageType::noInternetForTimeSync:
            return tr("No online server in the System has internet access for time synchronization.");

        case MessageType::emailIsEmpty:
            messageParts << tr("Email address is not set.") << tr("You cannot receive System notifications by email.");
            break;
        case MessageType::smtpIsNotSet:
            messageParts << tr("Email server is not set.") << tr("You cannot receive System notifications by email.");
            break;
        case MessageType::usersEmailIsEmpty:
            messageParts << tr("Some users have not set their email addresses.") << tr("They cannot receive System notifications by email.");
            break;
        case MessageType::archiveIntegrityFailed:
            messageParts << resourceName;
            break;
        case MessageType::noLicenses:
            messageParts << tr("You have no licenses.") << tr("You cannot record video from cameras.");
            break;
        case MessageType::archiveRebuildFinished:
            messageParts << tr("Rebuilding archive index is completed on the following Server:") << resourceName;
            break;
        case MessageType::archiveRebuildCanceled:
            messageParts << tr("Rebuilding archive index is canceled by user on the following Server:") << resourceName;
            break;
        case MessageType::metadataOnSystemStorage:
            messageParts << tr("Analytics data can take up large amounts of space.");
            messageParts << tr("We recommend choosing another location for it instead of the "
                "system partition.");
            break;
        default:
            break;
    }

    if (!messageParts.isEmpty())
        return messageParts.join('\n');

    /* Description is ended with a dot, title is not. */
    return messageText(messageType, resourceName) + '.';
}
