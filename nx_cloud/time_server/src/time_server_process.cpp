#include "time_server_process.h"

#include <nx/network/socket_common.h>
#include <nx/network/time/time_protocol_client.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/scope_guard.h>
#include <utils/common/systemerror.h>

#include "settings.h"
#include "time_protocol_server.h"
#include "time_server_app_info.h"

namespace nx {
namespace time_server {

namespace {
static const SocketAddress timeProtocolServerEndpoint =
    SocketAddress(HostAddress::anyHost, network::kTimeProtocolDefaultPort);
} // namespace

TimeServerProcess::TimeServerProcess(int argc, char **argv):
    m_argc(argc),
    m_argv(argv)
{
}

TimeServerProcess::~TimeServerProcess()
{
    // TODO: waiting for exec() to return
}

void TimeServerProcess::pleaseStop()
{
    m_processTerminationEvent.set_value();
}

void TimeServerProcess::setOnStartedEventHandler(
    nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> handler)
{
    m_startedEventHandler = std::move(handler);
}

int TimeServerProcess::exec()
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
        // Initializing.
        conf::Settings settings;
        settings.load(m_argc, m_argv);
        if (settings.isShowHelpRequested())
        {
            settings.printCmdLineArgsHelp();
            return 0;
        }

        utils::log::initialize(
            settings.logging(),
            settings.dataDir(),
            TimeServerAppInfo::applicationDisplayName(),
            "log_file",
            QnLog::MAIN_LOG_ID);

        TimeProtocolServer timeProtocolServer(
            false,
            network::NatTraversalSupport::disabled);

        if (!timeProtocolServer.bind(timeProtocolServerEndpoint))
        {
            const auto sysErrorCode = SystemError::getLastOSErrorCode();
            NX_LOGX(lm("Failed to bind to local endpoint %1. %2")
                .str(timeProtocolServerEndpoint)
                .arg(SystemError::toString(sysErrorCode)),
                cl_logERROR);
            return 1;
        }

        // TODO: #ak: process rights reduction should be done here.

        if (!timeProtocolServer.listen())
        {
            const auto sysErrorCode = SystemError::getLastOSErrorCode();
            NX_LOGX(lm("Failed to listen to local endpoint %1. %2")
                .str(timeProtocolServer.address())
                .arg(SystemError::toString(sysErrorCode)),
                cl_logERROR);
            return 2;
        }

        NX_LOGX(lm("Serving time protocol (rfc868) on %1")
            .str(timeProtocolServer.address()), 
            cl_logALWAYS);

        // Initialization succeeded.

        processStartResult = true;
        triggerOnStartedEventHandlerGuard.fire();

        m_processTerminationEvent.get_future().wait();
    }
    catch (const std::exception& e)
    {
        NX_LOGX(lm("Error starting. %1").arg(e.what()), cl_logERROR);
        return 3;
    }

    return 0;
}

} // namespace time_server
} // namespace nx
