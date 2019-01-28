#include "multiserver_request_helper.h"

#include <network/universal_tcp_listener.h>

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

bool verifyPasswordOrSetError(
    const QnRestConnectionProcessor* owner,
    const QString& currentPassword,
    QnJsonRestResult* result)
{
    if (currentPassword.isEmpty())
    {
        result->setError(QnJsonRestResult::CantProcessRequest, "currentPassword is required");
        return false;
    }

    const auto authenticator = QnUniversalTcpListener::authenticator(owner->owner());
    const auto authResult = authenticator->verifyPassword(
        owner->getForeignAddress().address, owner->accessRights(), currentPassword);

    if (authResult == Qn::Auth_OK)
        return true;

    result->setError(QnJsonRestResult::CantProcessRequest, Qn::toErrorMessage(authResult));
    return false;
}

} // namespace detail
