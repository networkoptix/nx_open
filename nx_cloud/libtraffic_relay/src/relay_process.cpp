#include "relay_process.h"

#include <nx/network/socket_common.h>
#include <nx/network/time/time_protocol_client.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>

#include "controller/controller.h"
#include "model/model.h"
#include "view/view.h"
#include "settings.h"
#include "libtraffic_relay_app_info.h"

namespace nx {
namespace cloud {
namespace relay {

RelayProcess::RelayProcess(int argc, char **argv):
    m_argc(argc),
    m_argv(argv)
{
}

RelayProcess::~RelayProcess()
{
    // TODO: waiting for exec() to return
}

void RelayProcess::pleaseStop()
{
    m_processTerminationEvent.set_value();
}

void RelayProcess::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> handler)
{
    m_startedEventHandler = std::move(handler);
}

int RelayProcess::exec()
{
    bool processStartResult = false;
    auto triggerOnStartedEventHandlerGuard = makeScopeGuard(
        [this, &processStartResult]
        {
            if (m_startedEventHandler)
                m_startedEventHandler(processStartResult);
        });

    try
    {
        conf::Settings settings;
        settings.load(m_argc, m_argv);
        if (settings.isShowHelpRequested())
        {
            settings.printCmdLineArgsHelp();
            return 0;
        }

        initializeLog(settings);

        Model model(settings);
        Controller controller(settings, &model);
        View view(settings, model, &controller);

        // TODO: #ak: process rights reduction should be done here.

        view.start();

        processStartResult = true;
        triggerOnStartedEventHandlerGuard.fire();

        m_processTerminationEvent.get_future().wait();

        return 0;
    }
    catch (const std::exception& e)
    {
        NX_LOGX(lm("Error starting. %1").arg(e.what()), cl_logERROR);
        return 3;
    }
}

void RelayProcess::initializeLog(const conf::Settings& settings)
{
    utils::log::initialize(
        settings.logging(),
        settings.dataDir(),
        TrafficRelayAppInfo::applicationDisplayName(),
        "log_file",
        QnLog::MAIN_LOG_ID);
}

} // namespace relay
} // namespace cloud
} // namespace nx
