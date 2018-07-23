#include "multiserver_request_helper.h"
#include <rest/server/fusion_rest_handler.h>

namespace detail {

    void checkUpdateStatusRemotely(
        QnCommonModule* commonModule,
        const QString& path,
        QList<nx::UpdateStatus>* reply,
        QnMultiserverRequestContext<QnEmptyRequestData>* context)
    {
        static const QString kOfflineMessage = "peer is offline";
        auto mergeFunction =
            [](
                const QnUuid& serverId,
                bool success,
                QList<nx::UpdateStatus>& reply,
                QList<nx::UpdateStatus>& outputReply)
        {
            if (success)
            {
                NX_ASSERT(reply.size() == 1);
                outputReply.append(reply[0]);
            }
            else
            {
                outputReply.append(
                    nx::UpdateStatus(serverId, nx::UpdateStatus::Code::offline, kOfflineMessage));
            }
        };

        detail::requestRemotePeers(commonModule, path, *reply, context, mergeFunction);
        auto offlineServers = QSet<QnMediaServerResourcePtr>::fromList(
            commonModule->resourcePool()->getAllServers(Qn::Offline));
        for (const auto offlineServer : offlineServers)
        {
            if (std::find_if(
                reply->cbegin(),
                reply->cend(),
                [&offlineServer](const nx::UpdateStatus& updateStatus)
            {
                return updateStatus.serverId == offlineServer->getId();
            }) != reply->cend())
            {
                continue;
            }
            reply->append(
                nx::UpdateStatus(
                    offlineServer->getId(),
                    nx::UpdateStatus::Code::offline,
                    kOfflineMessage));
        }
    }

QSet<QnMediaServerResourcePtr> allServers(QnCommonModule* commonModule)
{
    const auto systemName = commonModule->globalSettings()->systemName();
    auto servers = QSet<QnMediaServerResourcePtr>::fromList(commonModule->resourcePool()->getAllServers(Qn::Online));

    for (const auto& moduleInformation : commonModule->moduleDiscoveryManager()->getAll())
    {
        if (moduleInformation.systemName != systemName)
            continue;

        const auto server =
            commonModule->resourcePool()->getResourceById<QnMediaServerResource>(moduleInformation.id);
        if (!server)
            continue;

        servers.insert(server);
    }

    return servers;
}

} // namespace detail