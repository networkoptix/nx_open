#include "controller.h"

#include "relay/relay_cluster_client_factory.h"

namespace nx {
namespace hpm {

Controller::Controller(
    const conf::Settings& settings,
    nx::network::stun::MessageDispatcher* stunMessageDispatcher)
:
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
    m_mediaserverApi(m_cloudDataProvider.get(), stunMessageDispatcher),
    m_relayClusterClient(RelayClusterClientFactory::instance().create(settings)),
    m_listeningPeerPool(settings.listeningPeer()),
    m_listeningPeerRegistrator(
        settings,
        m_cloudDataProvider.get(),
        stunMessageDispatcher,
        &m_listeningPeerPool,
        m_relayClusterClient.get()),
    m_cloudConnectProcessor(
        settings,
        m_cloudDataProvider.get(),
        stunMessageDispatcher,
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

PeerRegistrator& Controller::listeningPeerRegistrator()
{
    return m_listeningPeerRegistrator;
}

const PeerRegistrator& Controller::listeningPeerRegistrator() const
{
    return m_listeningPeerRegistrator;
}

ListeningPeerPool& Controller::listeningPeerPool()
{
    return m_listeningPeerPool;
}

nx::cloud::discovery::RegisteredPeerPool& Controller::discoveredPeerPool()
{
    return m_discoveredPeerPool;
}

void Controller::stop()
{
    m_cloudConnectProcessor.stop();
}

} // namespace hpm
} // namespace nx
