#include "multiserver_request_helper.h"

namespace detail {

void checkUpdateStatusRemotely(const QList<QnUuid>& participants, QnCommonModule* commonModule,
    const QString& path, QList<nx::update::Status>* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    static const QString kOfflineMessage = "peer is offline";
    auto mergeFunction =
        [](const QnUuid& serverId, bool success, QList<nx::update::Status>& reply,
            QList<nx::update::Status>& outputReply)
        {
            if (success)
            {
                NX_ASSERT(reply.size() == 1);
                outputReply.append(reply[0]);
            }
            else
            {
                outputReply.append(nx::update::Status(serverId, nx::update::Status::Code::offline,
                    kOfflineMessage));
            }
        };

    requestRemotePeers(commonModule, path, *reply, context, mergeFunction, participants);
    auto offlineServers = QSet<QnMediaServerResourcePtr>::fromList(
        commonModule->resourcePool()->getAllServers(Qn::Offline));

    const auto participantsSet = QSet<QnUuid>::fromList(participants);
    for (const auto offlineServer: offlineServers)
    {
        if (!participantsSet.isEmpty() && !participantsSet.contains(offlineServer->getId()))
            continue;

        if (std::find_if(reply->cbegin(), reply->cend(),
            [&offlineServer](const nx::update::Status& updateStatus)
            {
                return updateStatus.serverId == offlineServer->getId();
            }) != reply->cend())
        {
            continue;
        }

        reply->append(nx::update::Status(offlineServer->getId(),nx::update::Status::Code::offline,
            kOfflineMessage));
    }
}

QSet<QnMediaServerResourcePtr> participantServers(const QList<QnUuid>& serverIdList,
    QnCommonModule* commonModule)
{
    const auto systemName = commonModule->globalSettings()->systemName();
    auto servers = QSet<QnMediaServerResourcePtr>::fromList(
        commonModule->resourcePool()->getAllServers(Qn::Online));

    for (const auto& moduleInformation: commonModule->moduleDiscoveryManager()->getAll())
    {
        if (moduleInformation.systemName != systemName)
            continue;

        const auto server = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
            moduleInformation.id);

        if (server)
            servers.insert(server);
    }

    return filterOutNonParticipants(servers, serverIdList);
}

QSet<QnMediaServerResourcePtr> filterOutNonParticipants(
    const QSet<QnMediaServerResourcePtr>& allServers, const QList<QnUuid>& serverIdList)
{
    QSet<QnMediaServerResourcePtr> resultServers = allServers;
    if (!serverIdList.isEmpty())
    {
        const auto serverIdSet = QSet<QnUuid>::fromList(serverIdList);
        for (auto it = resultServers.cbegin(); it != resultServers.cend();)
        {
            if (!serverIdSet.contains((*it)->getId()))
                it = resultServers.erase(it);
            else
                ++it;
        }
    }

    return resultServers;
}

} // namespace detail
