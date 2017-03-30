#include "updates_common.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <client/client_settings.h>
#include <network/module_finder.h>

namespace {

QString serverNamesString(const QnMediaServerResourceList& servers)
{
    QString result;

    for (const auto& server: servers)
    {
        if (!result.isEmpty())
            result += lit("\n");

        result.append(QnResourceDisplayInfo(server).toString(Qn::RI_WithUrl));
    }

    return result;
}

} // namespace

QSet<QnUuid> QnUpdateUtils::getServersLinkedToCloud(const QSet<QnUuid>& peers)
{
    QSet<QnUuid> result;

    const auto moduleFinder = qnModuleFinder;
    if (!moduleFinder)
        return result;

    for (const auto& id: peers)
    {
        const auto server =
            resourcePool()->getIncompatibleResourceById(id, false).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        const auto moduleInformation = moduleFinder->moduleInformation(server);
        if (!moduleInformation.cloudSystemId.isEmpty())
            result.insert(id);
    }

    return result;
}

QString QnUpdateResult::errorMessage() const
{
    switch (result)
    {
        case QnUpdateResult::Successful:
            return tr("Update has been successfully finished.");

        case QnUpdateResult::Cancelled:
            return tr("Update has been cancelled.");

        case QnUpdateResult::LockFailed:
            return tr("Another user has already started an update.");

        case QnUpdateResult::AlreadyUpdated:
            return tr("All servers are already updated.");

        case QnUpdateResult::DownloadingFailed:
            return tr("Could not download updates.");

        case QnUpdateResult::DownloadingFailed_NoFreeSpace:
            return tr("Could not download updates.")
                + lit("\n")
                + tr("No free space left on the disk.");

        case QnUpdateResult::UploadingFailed:
            return tr("Could not push updates to servers.")
                + lit("\n")
                + tr("The problem is caused by %n servers:", "", failedServers.size())
                + lit("\n")
                + serverNamesString(failedServers);

        case QnUpdateResult::UploadingFailed_NoFreeSpace:
            return tr("No free space left on %n servers:", "", failedServers.size())
                + lit("\n")
                + serverNamesString(failedServers);

        case QnUpdateResult::UploadingFailed_Timeout:
            return tr("%n servers are not responding:", "", failedServers.size())
                + lit("\n")
                + serverNamesString(failedServers);

        case QnUpdateResult::UploadingFailed_Offline:
            return tr("%n servers have gone offline:", "", failedServers.size())
                + lit("\n")
                + serverNamesString(failedServers);

        case QnUpdateResult::UploadingFailed_AuthenticationFailed:
            return tr("Authentication failed for %n servers:", "", failedServers.size())
                + lit("\n")
                + serverNamesString(failedServers);

        case QnUpdateResult::ClientInstallationFailed:
            return tr("Could not install an update to the client.");

        case QnUpdateResult::InstallationFailed:
        case QnUpdateResult::RestInstallationFailed:
            return tr("Could not install updates on one or more servers.");
    }

    return QString();
}
