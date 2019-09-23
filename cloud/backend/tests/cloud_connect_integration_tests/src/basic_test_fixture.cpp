#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

TrafficBlocker::TrafficBlocker(
    int connectionNumber,
    std::atomic<int>* blockedConnectionNumber)
    :
    m_connectionNumber(connectionNumber),
    m_blockedConnectionNumber(blockedConnectionNumber)
{
}

int TrafficBlocker::write(const void* data, size_t count)
{
    if (m_connectionNumber <= m_blockedConnectionNumber->load())
        return 0;

    return m_outputStream->write(data, count);
}

//-------------------------------------------------------------------------------------------------

void PredefinedCredentialsProvider::setCredentials(
    hpm::api::SystemCredentials cloudSystemCredentials)
{
    m_cloudSystemCredentials = std::move(cloudSystemCredentials);
}

std::optional<hpm::api::SystemCredentials>
    PredefinedCredentialsProvider::getSystemCredentials() const
{
    return m_cloudSystemCredentials;
}

//-------------------------------------------------------------------------------------------------

MemoryRemoteRelayPeerPool::MemoryRemoteRelayPeerPool(
    const nx::cloud::relay::conf::Settings& settings,
    PeerPoolEventHandler* relayTest)
    :
    base_type(settings),
    m_relayTest(relayTest)
{
}

void MemoryRemoteRelayPeerPool::addPeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    base_type::addPeer(
        domainName,
        [this, domainName, handler = std::move(handler)](bool added)
        {
            if (added)
                m_relayTest->peerAdded(domainName);
            handler(added);
        });
}

void MemoryRemoteRelayPeerPool::removePeer(
    const std::string& domainName,
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler)
{
    base_type::removePeer(
        domainName,
        [this, domainName, handler = std::move(handler)](bool removed)
        {
            if (removed)
                m_relayTest->peerRemoved(domainName);
            handler(removed);
        });
}

//-------------------------------------------------------------------------------------------------

MediatorConnectorCluster::Context::Context(
    nx::hpm::MediatorInstance& mediator,
    const QString& cloudHost)
    :
    mediator(mediator),
    connector(cloudHost.toStdString())
{
}

MediatorConnectorCluster::MediatorConnectorCluster(
    const nx::utils::Url& discoveryServiceUrl)
    :
    m_cluster(discoveryServiceUrl)
{
}

MediatorConnectorCluster::Context& MediatorConnectorCluster::addContext(
    const std::vector<const char*> args,
    int flags,
    const QString& testDir,
    const QString& cloudHost)
{
    auto context = std::make_unique<Context>(
        m_cluster.addMediator(args, flags, testDir),
        !cloudHost.isEmpty() ? cloudHost : AppInfo::defaultCloudHostName());

    m_mediators.emplace_back(std::move(context));
    return *m_mediators.back();
}

MediatorConnectorCluster::Context& MediatorConnectorCluster::context(int index)
{
    return *m_mediators[index];
}

nx::hpm::test::MediatorCluster& MediatorConnectorCluster::cluster()
{
    return m_cluster;
}

const nx::hpm::test::MediatorCluster& MediatorConnectorCluster::cluster() const
{
    return m_cluster;
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
