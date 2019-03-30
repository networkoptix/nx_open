#include "mediator_cluster.h"

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/mediator/mediator_service.h>

namespace nx::hpm::test {

Mediator& MediatorCluster::addMediator(
    std::vector<const char*> args,
    Mediator::MediatorTestFlags flags)
{
    auto& mediator = m_mediators.emplace_back(std::make_unique<Mediator>(flags));
    for (const char* arg : args)
        mediator->addArg(arg);

    return *mediator;
}

int MediatorCluster::size() const
{
    return static_cast<int>(m_mediators.size());
}

Mediator& MediatorCluster::mediator(int index)
{
    return *m_mediators[index];
}

const Mediator& MediatorCluster::mediator(int index) const
{
    return *m_mediators[index];
}

bool MediatorCluster::peerInformationSynchronizedInCluster(const std::string& peerDomainName)
{
    const auto doesMediatorHavePeer =
        [](Mediator* mediator, const std::string& peerDomainName)->bool
        {
            nx::utils::SyncQueue<bool> hasEndpoint;
            mediator->moduleInstance()->impl()->remoteMediatorPeerPool().findMediatorByPeerDomain(
                peerDomainName,
                [&hasEndpoint](MediatorEndpoint endpoint)
                {
                    hasEndpoint.push(!endpoint.domainName.empty());
                });
            return hasEndpoint.pop();
        };

    bool allMediatorsHavePeerDomainName = true;
    for (const auto& mediator : m_mediators)
        allMediatorsHavePeerDomainName &= doesMediatorHavePeer(mediator.get(), peerDomainName);

    return allMediatorsHavePeerDomainName;
}

} // namespace nx::hpm::test
