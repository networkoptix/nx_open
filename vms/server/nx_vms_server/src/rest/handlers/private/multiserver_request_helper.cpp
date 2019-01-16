#include "multiserver_request_helper.h"

namespace detail {

QSet<QnMediaServerResourcePtr> participantServers(
    const QList<QnUuid>& serverIdList,
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
    const QSet<QnMediaServerResourcePtr>& allServers,
    const QList<QnUuid>& serverIdList)
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
