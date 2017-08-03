#include <nx/network/public_ip_discovery.h>

#include <chrono>
#include <thread>
#include "controller.h"
#include "../model/model.h"

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
            &m_trafficRelay,
            discoverPublicIp()))
{
}

controller::AbstractConnectSessionManager& Controller::connectSessionManager()
{
    return *m_connectSessionManager;
}

std::string Controller::discoverPublicIp()
{
    network::PublicIPDiscovery ipDiscovery;

    while (true)
    {
        auto startPoint = std::chrono::steady_clock::now();

        ipDiscovery.update();
        ipDiscovery.waitForFinished();
        QHostAddress publicAddress = ipDiscovery.publicIP();

        if (!publicAddress.isNull())
            return publicAddress.toString().toStdString();

        NX_WARNING(this, "Failed to discover public relay host address. Keep trying...");

        auto endPoint = std::chrono::steady_clock::now();
        auto waitTime = endPoint - startPoint;
        auto kMinWaitTime = std::chrono::seconds(2);

        if (waitTime < kMinWaitTime)
            std::this_thread::sleep_for(kMinWaitTime - waitTime);
    }

    return "";
}


} // namespace relay
} // namespace cloud
} // namespace nx
