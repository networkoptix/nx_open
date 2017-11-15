#include "controller.h"

#include "relay_public_ip_discovery.h"
#include "statistics_provider.h"
#include "../model/model.h"
#include "../model/remote_relay_peer_pool.h"
#include "../settings.h"

namespace nx {
namespace cloud {
namespace relay {

Controller::Controller(
    const conf::Settings& settings,
    Model* model)
    :
    m_connectSessionManager(
        controller::ConnectSessionManagerFactory::create(
            settings,
            &model->clientSessionPool(),
            &model->listeningPeerPool(),
            &model->remoteRelayPeerPool(),
            &m_trafficRelay)),
    m_listeningPeerManager(
        relaying::ListeningPeerManagerFactory::instance().create(
            settings.listeningPeer(), &model->listeningPeerPool())),
    m_statisticsProvider(controller::StatisticsProviderFactory::instance().create(
        model->listeningPeerPool())),
    m_model(model),
    m_settings(&settings)
{
}

Controller::~Controller()
{
    for (const auto& subscriptionId: m_listeningPeerPoolSubscriptions)
        m_model->listeningPeerPool().peerConnectedSubscription().removeSubscription(subscriptionId);
}

controller::AbstractConnectSessionManager& Controller::connectSessionManager()
{
    return *m_connectSessionManager;
}

relaying::AbstractListeningPeerManager& Controller::listeningPeerManager()
{
    return *m_listeningPeerManager;
}

controller::AbstractStatisticsProvider& Controller::statisticsProvider()
{
    return *m_statisticsProvider;
}

const controller::AbstractStatisticsProvider& Controller::statisticsProvider() const
{
    return *m_statisticsProvider;
}

bool Controller::discoverPublicAddress()
{
    auto publicHostAddress = controller::PublicIpDiscoveryService::get();
    if (!(bool) publicHostAddress)
        return false;

    SocketAddress publicSocketAddress(*publicHostAddress, m_settings->http().endpoints.front().port);

    nx::utils::SubscriptionId subscriptionId;
    subscribeForPeerConnected(&subscriptionId, publicSocketAddress.toStdString());
    m_listeningPeerPoolSubscriptions.push_back(subscriptionId);

    subscribeForPeerDisconnected(&subscriptionId);
    m_listeningPeerPoolSubscriptions.push_back(subscriptionId);

    return true;
}

void Controller::subscribeForPeerConnected(
    nx::utils::SubscriptionId* subscriptionId,
    std::string publicAddress)
{
    m_model->listeningPeerPool().peerConnectedSubscription().subscribe(
        [this, publicAddress = std::move(publicAddress)](std::string peer)
        {
            m_model->remoteRelayPeerPool().addPeer(peer, publicAddress)
                .then(
                    [this, peer](cf::future<bool> addPeerFuture)
                    {
                        if (addPeerFuture.get())
                        {
                            NX_VERBOSE(this, lm("Successfully added peer %1 to RemoteRelayPool")
                                .arg(peer));
                        }
                        else
                        {
                            NX_VERBOSE(this, lm("Failed to add peer %1 to RemoteRelayPool")
                                .arg(peer));
                        }

                        return cf::unit();
                    });
        },
        subscriptionId);
}

void Controller::subscribeForPeerDisconnected(nx::utils::SubscriptionId* subscriptionId)
{
    m_model->listeningPeerPool().peerDisconnectedSubscription().subscribe(
        [this](std::string peer)
        {
            m_model->remoteRelayPeerPool().removePeer(peer)
                .then(
                    [this, peer](cf::future<bool> removePeerFuture)
                    {
                        if (removePeerFuture.get())
                        {
                            NX_VERBOSE(this, lm("Successfully removed peer %1 to RemoteRelayPool")
                                .arg(peer));
                        }
                        else
                        {
                            NX_VERBOSE(this, lm("Failed to remove peer %1 to RemoteRelayPool")
                                .arg(peer));
                        }

                        return cf::unit();
                    });
        },
        subscriptionId);
}



} // namespace relay
} // namespace cloud
} // namespace nx
