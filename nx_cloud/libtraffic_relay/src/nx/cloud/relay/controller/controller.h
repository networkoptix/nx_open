#pragma once

#include <memory>

#include <nx/utils/subscription.h>

#include <nx/cloud/relaying/listening_peer_manager.h>

#include "connect_session_manager.h"
#include "traffic_relay.h"

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }
class Model;
namespace controller { class AbstractStatisticsProvider; }

class Controller
{
public:
    Controller(
        const conf::Settings& settings,
        Model* model);
    virtual ~Controller();

    controller::AbstractTrafficRelay& trafficRelay();
    controller::AbstractConnectSessionManager& connectSessionManager();
    relaying::AbstractListeningPeerManager& listeningPeerManager();

    bool discoverPublicAddress();

private:
    std::unique_ptr<controller::AbstractTrafficRelay> m_trafficRelay;
    std::unique_ptr<controller::AbstractConnectSessionManager> m_connectSessionManager;
    std::unique_ptr<relaying::AbstractListeningPeerManager> m_listeningPeerManager;
    Model* m_model;
    const conf::Settings* m_settings;
    std::vector<nx::utils::SubscriptionId> m_listeningPeerPoolSubscriptions;

    void subscribeForPeerConnected(nx::utils::SubscriptionId* subscriptionId);
    void subscribeForPeerDisconnected(nx::utils::SubscriptionId* subscriptionId);
};

} // namespace relay
} // namespace cloud
} // namespace nx
