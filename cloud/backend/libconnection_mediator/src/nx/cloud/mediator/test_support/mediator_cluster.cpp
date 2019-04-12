#include "mediator_cluster.h"

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/mediator/mediator_service.h>

#include <nx/cloud/discovery/test_support/discovery_server.h>

namespace nx::hpm::test {

namespace {

static constexpr char kClusterId[] = "mediator_test_cluster";

static bool mediatorHasPeer(MediatorFunctionalTest* mediator, const std::string& peerDomainName)
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

MediatorFunctionalTest& MediatorCluster::addMediator(int flags, const QString& testDir)
{
    auto& mediator =
        m_mediators.emplace_back(std::make_unique<MediatorFunctionalTest>(flags, testDir));
    addClusterArgs(mediator.get());
    return *mediator;
}

MediatorFunctionalTest& MediatorCluster::addMediator(
    std::vector<const char*> args,
    int flags,
    const QString& testDir)
{
    auto& mediator = addMediator(flags, testDir);
    for (const char* arg : args)
        mediator.addArg(arg);
    return mediator;
}

int MediatorCluster::size() const
{
    return static_cast<int>(m_mediators.size());
}

MediatorFunctionalTest& MediatorCluster::mediator(int index)
{
    return *m_mediators[index];
}

const MediatorFunctionalTest& MediatorCluster::mediator(int index) const
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

void MediatorCluster::addClusterArgs(MediatorFunctionalTest* mediator)
{
    std::string nodeId = lm("mediator_%1").arg(m_mediators.size()).toStdString();
    std::string discoveryServiceUrl = m_discoveryServer.url().toStdString();

    mediator->addArg("-server/name", "127.0.0.1");
    mediator->addArg("-listeningPeerDb/connectionRetryDelay", "100ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/enabled", "true");
    mediator->addArg(
        "-listeningPeerDb/cluster/discovery/discoveryServiceUrl",
        discoveryServiceUrl.c_str());
    mediator->addArg("-listeningPeerDb/cluster/discovery/roundTripPadding", "1ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/registrationErrorDelay", "10ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/onlineNodesRequestDelay", "10ms");
    mediator->addArg("-listeningPeerDb/cluster/nodeConnectRetryTimeout", "100ms");
    mediator->addArg("-listeningPeerDb/cluster/clusterId", kClusterId);
    mediator->addArg("-listeningPeerDb/cluster/nodeId", nodeId.c_str());
}

} // namespace nx::hpm::test
