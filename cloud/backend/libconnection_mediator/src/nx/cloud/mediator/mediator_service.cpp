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
#include "public_ip_discovery.h"
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

std::string MediatorProcess::nodeId() const
{
    return m_controller->remoteMediatorPeerPool().nodeId();
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

    if (!registerThisInstanceNameInCluster(settings))
        return -1;

    const int result = runMainLoop();

    view.stop();
    controller.stop();

    NX_ALWAYS(this, lm("%1 is stopped")
        .arg(QnLibConnectionMediatorAppInfo::applicationDisplayName()));

    return result;
}

bool MediatorProcess::registerThisInstanceNameInCluster(const conf::Settings& settings)
{
    MediatorEndpoint endpoint;
    if (settings.server().name.empty())
    {
        const auto publicIp = m_controller->discoverPublicAddress();
        if (!publicIp)
        {
            NX_ERROR(this, "Failed to discover public address. Terminating.");
            return false;
        }
        endpoint.domainName = publicIp->toString().toStdString();
    }
    else
    {
        endpoint.domainName = settings.server().name;
    }

    endpoint.httpPort = httpEndpoints().front().port;
    endpoint.httpsPort = httpsEndpoints().front().port;
    endpoint.stunUdpPort = stunUdpEndpoints().front().port;

    m_controller->remoteMediatorPeerPool().setEndpoint(endpoint);

    if (settings.clusterDbMap().map.synchronizationSettings.discovery.enabled)
    {
        m_controller->remoteMediatorPeerPool().startDiscovery(
            &m_view->httpServer().messageDispatcher());
    }

    return true;
}

} // namespace hpm
} // namespace nx
