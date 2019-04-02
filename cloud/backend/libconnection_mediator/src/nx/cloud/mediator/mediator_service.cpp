#include "mediator_service.h"

#include <algorithm>
#include <iostream>
#include <list>

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
#include <nx/utils/platform/current_process.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>

#include "controller.h"
#include "libconnection_mediator_app_info.h"
#include "settings.h"
#include "statistics/statistics_provider.h"
#include "statistics/stats_manager.h"
#include "view.h"

namespace nx {
namespace hpm {

MediatorProcess::MediatorProcess(int argc, char **argv):
    base_type(argc, argv, QnLibConnectionMediatorAppInfo::applicationDisplayName())
{
}

std::vector<network::SocketAddress> MediatorProcess::httpEndpoints() const
{
    return m_view->httpServer().endpoints();
}

std::vector<network::SocketAddress> MediatorProcess::httpsEndpoints() const
{
    return m_view->httpServer().sslEndpoints();
}

std::vector<network::SocketAddress> MediatorProcess::stunUdpEndpoints() const
{
    return m_view->stunServer().udpEndpoints();
}

std::vector<network::SocketAddress> MediatorProcess::stunTcpEndpoints() const
{
    return m_view->stunServer().tcpEndpoints();
}

ListeningPeerPool* MediatorProcess::listeningPeerPool() const
{
    return &m_controller->listeningPeerPool();
}

Controller& MediatorProcess::controller()
{
    return *m_controller;
}

const Controller& MediatorProcess::controller() const
{
    return *m_controller;
}

ListeningPeerDb& MediatorProcess::listeningPeerDb()
{
    return m_controller->listeningPeerDb();
}

std::unique_ptr<nx::utils::AbstractServiceSettings> MediatorProcess::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int MediatorProcess::serviceMain(const nx::utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    nx::utils::TimerManager timerManager;

    NX_INFO(this, lm("Initializating controller"));

    Controller controller(settings);
    while (!controller.doMandatoryInitialization())
    {
        NX_INFO(this, lm("Retrying controller initialization after delay"));
        std::this_thread::sleep_for(settings.clusterDbMap().connectionRetryDelay);
    }
    m_controller = &controller;

    NX_INFO(this, lm("Initializating view"));

    View view(settings, &controller);
    m_view = &view;

    stats::Provider statisticsProvider(
        controller.statisticsManager(),
        view.httpServer().server(),
        view.stunServer().statisticsProvider());
    view.httpServer().registerStatisticsApiHandlers(statisticsProvider);

    NX_INFO(this, lm("Initializating view"));

    // Process privilege reduction.
    if (!settings.general().systemUserToRunUnder.isEmpty())
    {
        NX_INFO(this, lm("Changing active user"));
        nx::utils::CurrentProcess::changeUser(settings.general().systemUserToRunUnder);
    }

    NX_INFO(this, lm("Starting view"));

    view.start();

    NX_INFO(this, lm("Initialization completed. Running..."));

    registerThisInstanceNameInCluster(settings);

    const int result = runMainLoop();

    view.stop();
    controller.stop();

    NX_ALWAYS(this, lm("%1 is stopped")
        .arg(QnLibConnectionMediatorAppInfo::applicationDisplayName()));

    return result;
}

void MediatorProcess::registerThisInstanceNameInCluster(const conf::Settings& settings)
{
    const auto assignPort =
        [](const std::vector<network::SocketAddress>& endpoints, int* outPort)
        {
            *outPort = endpoints.empty() ? MediatorEndpoint::kPortUnused : endpoints.front().port;
        };

    if (settings.server().name.empty())
    {
        NX_INFO(this, "Server name is empty, discovery is not enabled.");
        return;
    }

    MediatorEndpoint endpoint;
    endpoint.domainName = settings.server().name;;
    assignPort(httpEndpoints(), &endpoint.httpPort);
    assignPort(httpsEndpoints(), &endpoint.httpsPort);
    assignPort(stunUdpEndpoints(), &endpoint.stunUdpPort);

    m_controller->listeningPeerDb().setThisMediatorEndpoint(endpoint);

    if (settings.clusterDbMap().map.synchronizationSettings.discovery.enabled)
    {
        m_controller->listeningPeerDb().startDiscovery(
            &m_view->httpServer().messageDispatcher());
    }
}

} // namespace hpm
} // namespace nx
