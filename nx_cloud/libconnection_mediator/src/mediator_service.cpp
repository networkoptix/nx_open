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
#include "http/get_listening_peer_list_handler.h"
#include "http/http_server.h"
#include "libconnection_mediator_app_info.h"
#include "settings.h"
#include "statistics/stats_manager.h"
#include "stun_server.h"

namespace nx {
namespace hpm {

MediatorProcess::MediatorProcess(int argc, char **argv):
    base_type(argc, argv, QnLibConnectionMediatorAppInfo::applicationDisplayName()),
    m_controller(nullptr),
    m_stunServer(nullptr),
    m_httpServer(nullptr)
{
}

std::vector<SocketAddress> MediatorProcess::httpEndpoints() const
{
    return m_httpServer->httpEndpoints();
}

std::vector<SocketAddress> MediatorProcess::stunEndpoints() const
{
    return m_stunServer->endpoints();
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

std::unique_ptr<nx::utils::AbstractServiceSettings> MediatorProcess::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int MediatorProcess::serviceMain(const nx::utils::AbstractServiceSettings& abstractSettings)
{
    const conf::Settings& settings = static_cast<const conf::Settings&>(abstractSettings);

    nx::utils::TimerManager timerManager;

    auto stunServer = std::make_unique<StunServer>(settings);
    m_stunServer = stunServer.get();

    // TODO: #ak Controller should be instanciated before stunServer.
    // That requires decoupling controller and STUN request handling.
    Controller controller(
        settings,
        &stunServer->dispatcher());
    m_controller = &controller;

    http::Server httpServer(
        settings,
        controller.listeningPeerRegistrator(),
        &controller.discoveredPeerPool());
    m_httpServer = &httpServer;

    // TODO: #ak Following call should be removed. 
    // Http server should be passed to stun server via constructor.
    stunServer->initializeHttpTunnelling(&httpServer);

    // process privilege reduction
    nx::utils::CurrentProcess::changeUser(settings.general().systemUserToRunUnder);

    stunServer->listen();
    httpServer.listen();

    const int result = runMainLoop();

    stunServer->stopAcceptingNewRequests();
    controller.stop();
    stunServer.reset();

    NX_LOGX(lit("%1 is stopped")
        .arg(QnLibConnectionMediatorAppInfo::applicationDisplayName()), cl_logALWAYS);

    return result;
}

} // namespace hpm
} // namespace nx
