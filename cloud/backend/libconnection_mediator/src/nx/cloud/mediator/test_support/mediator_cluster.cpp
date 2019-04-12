#include "mediator_cluster.h"

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/mediator/mediator_service.h>

#include <nx/cloud/discovery/test_support/discovery_server.h>

namespace nx::hpm::test {

namespace {

static constexpr char kClusterId[] = "mediator_test_cluster";

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

MediatorCluster::MediatorCluster()
{
    if (!m_discoveryServer.bindAndListen())
        throw std::runtime_error("Failed to initialize discovery server");
}

Mediator& MediatorCluster::addMediator(
    std::vector<const char*> args,
    Mediator::MediatorTestFlags flags)
{
    std::string nodeId = lm("mediator_%1").arg(m_mediators.size()).toStdString();
    std::string discoveryServiceUrl = m_discoveryServer.url().toStdString();

    auto& mediator = m_mediators.emplace_back(std::make_unique<Mediator>(flags));

    //m_mediators.back()->addArg("-https/listenOn", "127.0.0.1");
    //m_mediators.back()->addArg("-http/connectionInactivityTimeout", "10m");
    m_mediators.back()->addArg("-server/name", "127.0.0.1");
    m_mediators.back()->addArg("-listeningPeerDb/connectionRetryDelay", "100ms");
    m_mediators.back()->addArg("-listeningPeerDb/cluster/discovery/enabled", "true");
    m_mediators.back()->addArg(
        "-listeningPeerDb/cluster/discovery/discoveryServiceUrl",
        discoveryServiceUrl.c_str());
    m_mediators.back()->addArg("-listeningPeerDb/cluster/discovery/roundTripPadding", "1ms");
    m_mediators.back()->addArg("-listeningPeerDb/cluster/discovery/registrationErrorDelay", "10ms");
    m_mediators.back()->addArg("-listeningPeerDb/cluster/discovery/onlineNodesRequestDelay", "10ms");
    m_mediators.back()->addArg("-listeningPeerDb/cluster/nodeConnectRetryTimeout", "100ms");
    m_mediators.back()->addArg("-listeningPeerDb/cluster/clusterId", kClusterId);
    m_mediators.back()->addArg("-listeningPeerDb/cluster/nodeId", nodeId.c_str());

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
