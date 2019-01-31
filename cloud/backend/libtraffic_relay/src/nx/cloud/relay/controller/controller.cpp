#include "controller.h"

#include "relay_public_ip_discovery.h"
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
    m_trafficRelay(controller::TrafficRelayFactory::instance().create()),
    m_connectSessionManager(
        controller::ConnectSessionManagerFactory::create(
            settings,
            &model->clientSessionPool(),
            &model->listeningPeerPool(),
            &model->remoteRelayPeerPool(),
            m_trafficRelay.get())),
    m_listeningPeerManager(
        relaying::ListeningPeerManagerFactory::instance().create(
            settings.listeningPeer(), &model->listeningPeerPool())),
    m_model(model),
    m_settings(settings)
{
}

controller::AbstractTrafficRelay& Controller::trafficRelay()
{
    return *m_trafficRelay;
}

controller::AbstractConnectSessionManager& Controller::connectSessionManager()
{
    return *m_connectSessionManager;
}

relaying::AbstractListeningPeerManager& Controller::listeningPeerManager()
{
    return *m_listeningPeerManager;
}

std::optional<network::HostAddress> Controller::discoverPublicAddress()
{
    auto publicHostAddress = controller::PublicIpDiscoveryService::get();
    if (!(bool)publicHostAddress)
        return std::nullopt;

    return *publicHostAddress;
}

} // namespace relay
} // namespace cloud
} // namespace nx
