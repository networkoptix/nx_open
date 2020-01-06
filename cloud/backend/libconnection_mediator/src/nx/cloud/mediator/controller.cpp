#include "controller.h"

#include "geo_ip/resolver_factory.h"
#include "relay/relay_cluster_client_factory.h"

namespace nx {
namespace hpm {

Controller::Controller(const conf::Settings& settings):
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
    m_geoIpResolver(geo_ip::ResolverFactory::instance().create(settings)),
    m_relayClusterClient(RelayClusterClientFactory::instance().create(settings, m_geoIpResolver.get())),
    m_listeningPeerDb(settings),
    m_listeningPeerPool(settings.listeningPeer(), &m_listeningPeerDb),
    m_listeningPeerRegistrator(
        settings,
        m_cloudDataProvider.get(),
        &m_listeningPeerPool,
        m_relayClusterClient.get()),
    m_statsManager(
        settings,
        m_listeningPeerPool,
        m_listeningPeerRegistrator),
    m_cloudConnectProcessor(
        settings,
        m_cloudDataProvider.get(),
        &m_listeningPeerPool,
        m_relayClusterClient.get(),
        &m_statsManager.collector()),
    m_discoveredPeerPool(settings.discovery())
{
    if (!m_cloudDataProvider)
        NX_INFO(this, lit("STUN Server is running without cloud (debug mode)"));
}

Controller::~Controller() = default;

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

AbstractCloudDataProvider& Controller::cloudDataProvider()
{
    return *m_cloudDataProvider;
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

ListeningPeerDb& Controller::listeningPeerDb()
{
    return m_listeningPeerDb;
}

const ListeningPeerDb& Controller::listeningPeerDb() const
{
    return m_listeningPeerDb;
}

const stats::StatsManager& Controller::statisticsManager() const
{
    return m_statsManager;
}

nx::geo_ip::AbstractResolver& Controller::geoIpResolver()
{
    return *m_geoIpResolver;
}

bool Controller::doMandatoryInitialization()
{
    return m_listeningPeerDb.initialize();
}

} // namespace hpm
} // namespace nx
