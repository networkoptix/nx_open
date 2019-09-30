#include "mediator_cluster.h"

#include <nx/cloud/discovery/test_support/discovery_server.h>
#include <nx/cloud/mediator/mediator_service.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::hpm::test {

namespace {

static constexpr char kClusterId[] = "mediator_test_cluster";

static MediatorEndpoint lookupMediatorEndpointSync(
    MediatorInstance* mediator,
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

MediatorCluster::MediatorCluster(const nx::utils::Url& discoveryServiceUrl):
    m_discoveryServiceUrl(discoveryServiceUrl)
{
}

MediatorCluster::~MediatorCluster()
{
    stop();
    if (m_factoryFuncToRestore)
    {
        AbstractCloudDataProviderFactory::setFactoryFunc(
            std::move(*m_factoryFuncToRestore));
        m_factoryFuncToRestore.reset();
    }
}

void MediatorCluster::stop()
{
    for (auto& mediator : m_mediators)
        mediator->stop();
}

MediatorInstance& MediatorCluster::addMediator(int flags, QString testDir)
{
    int index = size();

    if (testDir.isEmpty())
        testDir = testDataDir() + QString("/mediator_%1").arg(index);

    if (flags & MediatorInstance::initializeSocketGlobals)
        nx::network::SocketGlobals::cloud().reinitialize();

    if (flags & MediatorInstance::useTestCloudDataProvider)
        registerCloudDataProvider(&m_cloudDataProvider);


    m_mediators.emplace_back(std::make_unique<MediatorInstance>(flags, testDir));
    addClusterArgs(index, m_mediators.back().get());
    return *m_mediators.back();
}

MediatorInstance& MediatorCluster::addMediator(
    std::vector<const char*> args,
    int flags,
    QString testDir)
{
    auto& mediator = addMediator(flags, testDir);
    for (const char* arg : args)
        mediator.addArg(arg);
    return mediator;
}

AbstractCloudDataProvider::System MediatorCluster::addRandomSystem()
{
    AbstractCloudDataProvider::System system(
        QnUuid::createUuid().toSimpleString().toUtf8(),
        nx::utils::generateRandomName(16),
        true);
    m_cloudDataProvider.addSystem(
        system.id,
        system);
    return system;
}

int MediatorCluster::size() const
{
    return static_cast<int>(m_mediators.size());
}

MediatorInstance& MediatorCluster::mediator(int index)
{
    return *m_mediators[index];
}

const MediatorInstance& MediatorCluster::mediator(int index) const
{
    return *m_mediators[index];
}

bool MediatorCluster::peerInformationSynchronizedInCluster(const std::string& peerDomainName) const
{
    for (const auto& mediator : m_mediators)
    {
        if (lookupMediatorEndpointSync(mediator.get(), peerDomainName).domainName.empty())
            return false;
    }

    return true;
}

bool MediatorCluster::peerInformationIsAbsentFromCluster(const std::string& peerDomainName) const
{
    for (const auto& mediator : m_mediators)
    {
        if (!lookupMediatorEndpointSync(mediator.get(), peerDomainName).domainName.empty())
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

std::vector<MediatorEndpoint> MediatorCluster::endpoints() const
{
    std::vector<MediatorEndpoint> mediatorEndpoints;
    for (const auto& mediator : m_mediators)
    {
        mediatorEndpoints.push_back(
            mediator->moduleInstance()->impl()->listeningPeerDb().thisMediatorEndpoint());
    }
    return mediatorEndpoints;
}

void MediatorCluster::addClusterArgs(int index, MediatorInstance* mediator)
{
    std::string nodeId = lm("mediator_%1").arg(index).toStdString();

    mediator->addArg("-server/name", "127.0.0.1");
    mediator->addArg("-listeningPeerDb/connectionRetryDelay", "100ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/enabled", "true");
    mediator->addArg(
        "-listeningPeerDb/cluster/discovery/discoveryServiceUrl",
        m_discoveryServiceUrl.toStdString().c_str());
    mediator->addArg("-listeningPeerDb/cluster/discovery/roundTripPadding", "1ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/registrationErrorDelay", "10ms");
    mediator->addArg("-listeningPeerDb/cluster/discovery/onlineNodesRequestDelay", "10ms");
    mediator->addArg("-listeningPeerDb/cluster/nodeConnectRetryTimeout", "100ms");
    mediator->addArg("-listeningPeerDb/cluster/clusterId", kClusterId);
    mediator->addArg("-listeningPeerDb/cluster/nodeId", nodeId.c_str());
}

void MediatorCluster::registerCloudDataProvider(AbstractCloudDataProvider* cloudDataProvider)
{
    if (m_factoryFuncToRestore)
        return;

    auto oldFactoryFunc =
        AbstractCloudDataProviderFactory::setFactoryFunc(
            [cloudDataProvider](auto&&...)
    {
        return std::make_unique<CloudDataProviderStub>(cloudDataProvider);
    });

    m_factoryFuncToRestore = std::move(oldFactoryFunc);
}

} // namespace nx::hpm::test
