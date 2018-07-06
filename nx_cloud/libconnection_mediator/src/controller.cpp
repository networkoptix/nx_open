#include "controller.h"

#include "relay/relay_cluster_client_factory.h"

namespace nx {
namespace hpm {

Controller::Controller(const conf::Settings& settings):
    m_statsManager(settings),
    m_cloudDataProvider(
        settings.cloudDB().runWithCloud
        ? AbstractCloudDataProviderFactory::create(
            settings.cloudDB().url,
            settings.cloudDB().user.toStdString(),
            settings.cloudDB().password.toStdString(),
            settings.cloudDB().updateInterval,
            settings.cloudDB().startTimeout)
        : nullptr),
    m_mediaserverEndpointTester(m_cloudDataProvider.get()),
    m_relayClusterClient(RelayClusterClientFactory::instance().create(settings)),
    m_listeningPeerPool(settings.listeningPeer()),
    m_listeningPeerRegistrator(
        settings,
        m_cloudDataProvider.get(),
        &m_listeningPeerPool,
        m_relayClusterClient.get()),
    m_cloudConnectProcessor(
        settings,
        m_cloudDataProvider.get(),
        &m_listeningPeerPool,
        m_relayClusterClient.get(),
        &m_statsManager.collector()),
    m_discoveredPeerPool(settings.discovery())
{
    if (!m_cloudDataProvider)
    {
        NX_LOGX(lit("STUN Server is running without cloud (debug mode)"), cl_logALWAYS);
    }
}

MediaserverEndpointTester& Controller::mediaserverEndpointTester()
{
    return m_mediaserverEndpointTester;
}

ListeningPeerPool& Controller::listeningPeerPool()
{
    return m_listeningPeerPool;
}

PeerRegistrator& Controller::listeningPeerRegistrator()
{
    return m_listeningPeerRegistrator;
}

const PeerRegistrator& Controller::listeningPeerRegistrator() const
{
    return m_listeningPeerRegistrator;
}

HolePunchingProcessor& Controller::cloudConnectProcessor()
{
    return m_cloudConnectProcessor;
}

nx::cloud::discovery::RegisteredPeerPool& Controller::discoveredPeerPool()
{
    return m_discoveredPeerPool;
}

const nx::cloud::discovery::RegisteredPeerPool& Controller::discoveredPeerPool() const
{
    return m_discoveredPeerPool;
}

void Controller::stop()
{
    m_cloudConnectProcessor.stop();
}

} // namespace hpm
} // namespace nx
