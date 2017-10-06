#pragma once

#include <nx/utils/subscription.h>

#include <nx/cloud/relaying/listening_peer_manager.h>

#include "connect_session_manager.h"
#include "traffic_relay.h"

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }
class Model;

class Controller
{
public:
    Controller(
        const conf::Settings& settings,
        Model* model);

    ~Controller();

    controller::AbstractConnectSessionManager& connectSessionManager();
    relaying::AbstractListeningPeerManager& listeningPeerManager();
    bool discoverPublicAddress();

private:
    controller::TrafficRelay m_trafficRelay;
    std::unique_ptr<controller::AbstractConnectSessionManager> m_connectSessionManager;
    std::unique_ptr<relaying::AbstractListeningPeerManager> m_listeningPeerManager;
    Model* m_model;
    const conf::Settings* m_settings;
    std::vector<nx::utils::SubscriptionId> m_listeningPeerPoolSubscriptions;

    void subscribeForPeerConnected(
        nx::utils::SubscriptionId* subscriptionId,
        std::string publicAddress);

    void subscribeForPeerDisconnected(nx::utils::SubscriptionId* subscriptionId);
};

} // namespace relay
} // namespace cloud
} // namespace nx
