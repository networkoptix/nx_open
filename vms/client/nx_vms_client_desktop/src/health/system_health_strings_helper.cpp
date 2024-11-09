// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_strings_helper.h"

#include <core/resource/camera_resource.h>
#include <nx/branding.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/common/utils/cloud_url_helper.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client;

namespace {

static const int kMaxResourcesLines = 3;

QStringList getCameraList(const QSet<QnResourcePtr>& resources)
{
    QStringList result;
    for (const auto& cameraResource: resources)
    {
        const auto camera = cameraResource.dynamicCast<QnVirtualCameraResource>();
        if (!NX_ASSERT(camera))
            continue;

        result << camera->getName();
    }
    return result;
}

} // namespace

QString QnSystemHealthStringsHelper::messageShortTitle(
    nx::vms::client::core::SystemContext* systemContext,
    MessageType messageType)
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
            return tr("The Site has no internet access for time synchronization");
        case MessageType::cameraRecordingScheduleIsInvalid:
            return tr("Camera recording schedule is invalid");
        case MessageType::metadataStorageNotSet:
            return tr("Storage for analytics data is not set");
        case MessageType::metadataOnSystemStorage:
            return tr("System partition is used for analytics data");
        case MessageType::saasLocalRecordingServicesOverused:
            return tr("Local recording services overused");
        case MessageType::saasCloudStorageServicesOverused:
            return tr("Cloud storage services overused");
        case MessageType::saasIntegrationServicesOverused:
            return tr("Paid integrations services overused");
        case MessageType::saasInSuspendedState:
            return tr("Site suspended");
        case MessageType::saasInShutdownState:
            return tr("Site shut down");
        case MessageType::saasTierIssue:
        {
            const QString kDefaultMessage = tr("Site will stop functioning soon");
            if (!systemContext)
                return kDefaultMessage;
            auto saas = systemContext->saasServiceManager();
            auto daysLeft = saas->tierGracePeriodDaysLeft();

            if (!daysLeft.has_value())
                return kDefaultMessage;
            if (daysLeft == 0)
            {
                if (saas->isTierGracePeriodExpired())
                    return tr("Site has stopped functioning");
                return tr("Site will stop functioning today");
            }
            if (daysLeft == 1)
                return tr("Site will stop functioning tomorrow");
            return tr("Site will stop functioning in %n days", "", *daysLeft);
        }
        case MessageType::showIntercomInformer:
            return tr("Intercom call");
        case MessageType::showMissedCallInformer:
            return tr("Intercom missed call");
        case MessageType::rejectIntercomCall:
            return tr("Reject intercom call");
        case MessageType::notificationLanguageDiffers:
            return tr("Notification and interface languages differ");

        default:
            break;
    }
    NX_ASSERT(false, "Requesting name for non-visual action: %1", messageType);
    return QString();
}

QString QnSystemHealthStringsHelper::messageNotificationTitle(
    nx::vms::client::core::SystemContext* systemContext,
    MessageType messageType,
   const QSet<QnResourcePtr>& resources)
{
    namespace html = nx::vms::common::html;

    switch (messageType)
    {
        case MessageType::emailIsEmpty:
            return tr("Email address is not set for your account");
        case MessageType::showIntercomInformer:
            return tr("Calling...");
        case MessageType::showMissedCallInformer:
            return tr("Missed call");
        case MessageType::storagesNotConfigured:
        {
            return resources.size() <= 1
                ? tr("Storage is not configured")
                : tr("Storage is not configured on %n servers", "", resources.size());
        }

        case MessageType::backupStoragesNotConfigured:
        {
            return resources.size() <= 1
                ? tr("Backup storage is not configured")
                : tr("Backup storage is not configured on %n servers", "", resources.size());
        }

        case MessageType::cameraRecordingScheduleIsInvalid:
        {
            return resources.size() <= 1
                ? tr("Recording schedule is invalid")
                : tr("Recording schedule is invalid on %n cameras", "", resources.size());
        }

        case MessageType::usersEmailIsEmpty:
        {
            return resources.size() <= 1
                ? tr("Email address is not set")
                : tr("Email address is not set for %n users", "", resources.size());
        }

        case MessageType::cloudPromo:
        {
            return tr("Check out %1",
                    "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName());
        }

        case MessageType::defaultCameraPasswords:
            return tr("Some cameras require passwords to be set");

        case MessageType::noInternetForTimeSync:
            return tr("No server has internet access for time synchronization");

        case MessageType::remoteArchiveSyncError:
            return tr("Remote archive synchronization failed");

        case MessageType::metadataStorageNotSet:
            return tr("Storage for analytics data is not set on %n Servers", "",
                resources.size());

        case MessageType::metadataOnSystemStorage:
        {
            return resources.size() <= 1
                ? tr("System storage is used for analytics data")
                : tr("System storage is used for analytics data on %n servers",
                      "",
                      resources.size());
        }

        default:
            break;
    }
    return messageShortTitle(systemContext, messageType);
}

QString QnSystemHealthStringsHelper::messageDescription(MessageType messageType)
{
    switch (messageType)
    {
        case MessageType::cloudPromo:
        {
            using nx::vms::utils::SystemUri;
            core::CloudUrlHelper urlHelper(
                SystemUri::ReferralSource::DesktopClient, SystemUri::ReferralContext::None);

            static const QString kTemplate =
                QString::fromLatin1("<style>td {padding-top: %4px; padding-right: %4px}</style>"
                                    "<p>%1</p>"
                                    "<table cellpadding='0' cellspacing='0'>"
                                    "<tr><td>%2</td><td>%3</td></tr>"
                                    "</table>");

            return kTemplate.arg(tr("Connect to your Site from anywhere!"))
                .arg(nx::vms::common::html::link(tr("Learn more"), urlHelper.aboutUrl()))
                .arg(nx::vms::common::html::customLink(
                    tr("Connect"), "settings")) //< TODO: #sivanov Replace to `#settings`.
                .arg(nx::style::Metrics::kStandardPadding);
        }
        case MessageType::notificationLanguageDiffers:
            return tr("Notifications language differs from the interface language");
        default:
            return {};
    }
}

QString QnSystemHealthStringsHelper::messageTooltip(
    nx::vms::client::core::SystemContext* systemContext,
    MessageType messageType,
    const QSet<QnResourcePtr>& resources)
{
    QStringList messageParts;

    switch (messageType)
    {
        // disable tooltip for promo
        case MessageType::cloudPromo:
            return QString();

        case MessageType::defaultCameraPasswords:
        {
            const auto cameras = getCameraList(resources);
            return cameras.size() > kMaxResourcesLines ? cameras.join("\n") : QString{};
        }

        case MessageType::archiveIntegrityFailed:
        case MessageType::archiveRebuildFinished:
        case MessageType::archiveRebuildCanceled:
            return QString();

        case MessageType::storagesNotConfigured:
        case MessageType::backupStoragesNotConfigured:
        case MessageType::metadataStorageNotSet:
            return QString(); //< Server list is displayed separately.

        case MessageType::noInternetForTimeSync:
            return tr("No online server in the site has internet access for time synchronization.");

        case MessageType::emailIsEmpty:
            messageParts << tr("Email address is not set.")
                << tr("You cannot receive Site notifications by email.");
            break;
        case MessageType::smtpIsNotSet:
            messageParts << tr("Email server is not set.")
                << tr("You cannot receive Site notifications by email.");
            break;
        case MessageType::usersEmailIsEmpty:
            messageParts << tr("Some users have not set their email addresses.")
                << tr("They cannot receive Site notifications by email.");
            break;
        case MessageType::noLicenses:
            messageParts << tr("You have no licenses.") << tr("You cannot record video from cameras.");
            break;
        case MessageType::metadataOnSystemStorage:
            messageParts << tr("Analytics data can take up large amounts of space.");
            messageParts << tr("We recommend choosing another location for it instead of the "
                "system partition.");
            break;
        case MessageType::cameraRecordingScheduleIsInvalid:
        {
            messageParts << tr("Some cameras are set to record in a mode they do not support.");
            messageParts << ""; //< Additional line break by design.
            messageParts += getCameraList(resources);
        }

        default:
            break;
    }

    if (!messageParts.isEmpty())
        return messageParts.join('\n');

    /* Description is ended with a dot, title is not. */
    return messageNotificationTitle(systemContext, messageType, resources) + '.';
}

QString QnSystemHealthStringsHelper::resourceText(
    const QStringList& list, int maxWidth, int maxResourcesLines)
{
    if (list.empty())
        return {};

    QStringList formattedResources;
    for (int i = 0; i < list.size() && i < maxResourcesLines; ++i)
        formattedResources.append(nx::utils::elideString(list[i], maxWidth, "(...)"));

    if (list.size() > maxResourcesLines)
    {
        const auto numberOfHiddenResources = list.size() - maxResourcesLines;
        const auto coloredLine = nx::vms::common::html::colored(
            tr("+ %n more", "", numberOfHiddenResources), core::colorTheme()->color("light16"));
        formattedResources.append(coloredLine);
    }
    return QString("<html><body>%1</body></html>").arg(formattedResources.join("<br>"));
}
