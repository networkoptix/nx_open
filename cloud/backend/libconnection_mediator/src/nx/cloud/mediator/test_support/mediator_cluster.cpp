#include "mediator_cluster.h"

#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/mediator/mediator_service.h>

#include <nx/cloud/discovery/test_support/discovery_server.h>

namespace nx::hpm::test {

namespace {

static constexpr char kClusterId[] = "mediator_test_cluster";

static MediatorEndpoint lookupMediatorEndpointSync(
    MediatorFunctionalTest* mediator,
    const std::string& peerDomainName)
{
    nx::utils::SyncQueue<MediatorEndpoint> hasEndpoint;
    mediator->moduleInstance()->impl()->listeningPeerDb().findMediatorByPeerDomain(
        peerDomainName,
        [&hasEndpoint](MediatorEndpoint endpoint)
        {
            hasEndpoint.push(endpoint);
        });
    return hasEndpoint.pop();
}

} //namespace

MediatorCluster::MediatorCluster()
{
    NX_ASSERT(m_discoveryServer.bindAndListen(), "Failed to initialize discovery server");
}

MediatorFunctionalTest& MediatorCluster::addMediator(int flags, const QString& testDir)
{
    m_mediators.emplace_back(std::make_unique<MediatorFunctionalTest>(flags, testDir));
    addClusterArgs(m_mediators.back().get());
    return *m_mediators.back();
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
        if (!lookupMediatorEndpointSync(mediator.get(), peerDomainName).domainName.empty())
            return false;
    }

    return true;
}

bool MediatorCluster::peerInformationIsAbsentFromCluster(const std::string& peerDomainName) const
{
    for (const auto& mediator : m_mediators)
    {
        if (lookupMediatorEndpointSync(mediator.get(), peerDomainName).domainName.empty())
            return false;
    }

    return true;
}

std::optional<MediatorEndpoint> MediatorCluster::lookupMediatorEndpoint(
    const std::string& peerDomainName) const
{
    for (const auto& mediator : m_mediators)
    {
        auto endpoint = lookupMediatorEndpointSync(mediator.get(), peerDomainName);
        if (!endpoint.domainName.empty())
            return endpoint;
    }
    return std::nullopt;
}

void MediatorCluster::addClusterArgs(MediatorFunctionalTest* mediator)
{
    std::string nodeId = lm("mediator_%1").arg(m_mediators.size()).toStdString();

    mediator->addArg("-server/name", "127.0.0.1");
    mediator->addArg("-listeningPeerDb/connectionRetryDelay", "100ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/enabled", "true");
    mediator->addArg(
        "-listeningPeerDb/cluster/discovery/discoveryServiceUrl",
        m_discoveryServer.url().toStdString().c_str());
    mediator->addArg("-listeningPeerDb/cluster/discovery/roundTripPadding", "1ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/registrationErrorDelay", "10ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/onlineNodesRequestDelay", "10ms");
    mediator->addArg("-listeningPeerDb/cluster/nodeConnectRetryTimeout", "100ms");
    mediator->addArg("-listeningPeerDb/cluster/clusterId", kClusterId);
    mediator->addArg("-listeningPeerDb/cluster/nodeId", nodeId.c_str());
}

} // namespace nx::hpm::test
