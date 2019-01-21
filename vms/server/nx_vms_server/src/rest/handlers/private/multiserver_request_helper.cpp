#include "multiserver_request_helper.h"

namespace detail {

QSet<QnMediaServerResourcePtr> participantServers(
    const IfParticipantPredicate& ifPartcipantPredicate,
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

    if (ifPartcipantPredicate)
    {
        for (auto it = servers.cbegin(); it != servers.cend();)
        {
            const auto participationStatus =
                ifPartcipantPredicate((*it)->getId(), (*it)->getModuleInformation().version);

            if (participationStatus != ParticipationStatus::participant)
                it = servers.erase(it);
            else
                ++it;
        }
    }

    return servers;
}

} // namespace detail
