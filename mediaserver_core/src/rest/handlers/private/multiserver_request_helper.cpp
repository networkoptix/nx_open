#include "multiserver_request_helper.h"

namespace detail {

void checkUpdateStatusRemotely(
    QnCommonModule* commonModule,
    const QString& path,
    QList<nx::update::Status>* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    static const QString kOfflineMessage = "peer is offline";
    auto mergeFunction =
        [](
            const QnUuid& serverId,
            bool success,
            QList<nx::update::Status>& reply,
            QList<nx::update::Status>& outputReply)
    {
        if (success)
        {
            NX_ASSERT(reply.size() == 1);
            outputReply.append(reply[0]);
        }
        else
        {
            outputReply.append(
                nx::update::Status(serverId, nx::update::Status::Code::offline, kOfflineMessage));
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
            [&offlineServer](const nx::update::Status& updateStatus)
            {
                return updateStatus.serverId == offlineServer->getId();
            }) != reply->cend())
        {
            continue;
        }
        reply->append(
            nx::update::Status(
                offlineServer->getId(),
                nx::update::Status::Code::offline,
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
