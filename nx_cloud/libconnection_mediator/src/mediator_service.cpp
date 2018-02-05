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
    return m_view->httpEndpoints();
}

std::vector<network::SocketAddress> MediatorProcess::stunEndpoints() const
{
    return m_view->stunEndpoints();
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

    NX_INFO(this, lm("Initializating controller"));

    Controller controller(settings);
    m_controller = &controller;

    NX_INFO(this, lm("Initializating view"));

    View view(settings, &controller);
    m_view = &view;

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

    const int result = runMainLoop();

    view.stop();
    controller.stop();

    NX_ALWAYS(this, lm("%1 is stopped")
        .arg(QnLibConnectionMediatorAppInfo::applicationDisplayName()));

    return result;
}

} // namespace hpm
} // namespace nx
