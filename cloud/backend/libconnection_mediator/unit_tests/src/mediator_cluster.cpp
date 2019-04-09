#include "mediator_cluster.h"

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/mediator/mediator_service.h>

namespace nx::hpm::test {

namespace {

static bool mediatorHasPeer(Mediator* mediator, const std::string& peerDomainName)
{
    nx::utils::SyncQueue<bool> hasEndpoint;
    mediator->moduleInstance()->impl()->listeningPeerDb().findMediatorByPeerDomain(
        peerDomainName,
        [&hasEndpoint](MediatorEndpoint endpoint)
        {
            hasEndpoint.push(!endpoint.domainName.empty());
        });
    return hasEndpoint.pop();
}

} //namespace

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

bool MediatorCluster::peerInformationSynchronizedInCluster(const std::string& peerDomainName) const
{
    for (const auto& mediator : m_mediators)
    {
        if (!mediatorHasPeer(mediator.get(), peerDomainName))
            return false;
    }

    return true;
}

bool MediatorCluster::peerInformationIsAbsentFromCluster(const std::string& peerDomainName) const
{
    for (const auto& mediator : m_mediators)
    {
        if (mediatorHasPeer(mediator.get(), peerDomainName))
            return false;
    }

    return true;
}

} // namespace nx::hpm::test
