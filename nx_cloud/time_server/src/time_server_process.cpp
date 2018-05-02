#include "time_server_process.h"

#include <nx/network/socket_common.h>
#include <nx/network/time/time_protocol_client.h>
#include <nx/network/time/time_protocol_server.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_initializer.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>

#include "settings.h"
#include "time_server_app_info.h"

namespace nx {
namespace time_server {

namespace {
static const network::SocketAddress kTimeProtocolServerEndpoint =
    network::SocketAddress(network::HostAddress::anyHost, network::kTimeProtocolDefaultPort);
} // namespace

TimeServerProcess::TimeServerProcess(int argc, char **argv):
    base_type(argc, argv, "TimeServer")
{
}

TimeServerProcess::~TimeServerProcess()
{
    // TODO: waiting for exec() to return
}

std::unique_ptr<nx::utils::AbstractServiceSettings> TimeServerProcess::createSettings()
{
    return std::make_unique<conf::Settings>();
}

int TimeServerProcess::serviceMain(
    const nx::utils::AbstractServiceSettings& /*abstractSettings*/)
{
    try
    {
        nx::network::TimeProtocolServer timeProtocolServer(false);
        if (!timeProtocolServer.bind(kTimeProtocolServerEndpoint))
        {
            const auto sysErrorCode = SystemError::getLastOSErrorCode();
            NX_LOGX(lm("Failed to bind to local endpoint %1. %2")
                .arg(kTimeProtocolServerEndpoint)
                .arg(SystemError::toString(sysErrorCode)),
                cl_logERROR);
            return 1;
        }

        // TODO: #ak: process rights reduction should be done here.

        if (!timeProtocolServer.listen())
        {
            const auto sysErrorCode = SystemError::getLastOSErrorCode();
            NX_LOGX(lm("Failed to listen to local endpoint %1. %2")
                .arg(timeProtocolServer.address())
                .arg(SystemError::toString(sysErrorCode)),
                cl_logERROR);
            return 2;
        }

        NX_LOGX(lm("Serving time protocol (rfc868) on %1")
            .arg(timeProtocolServer.address()), 
            cl_logALWAYS);

        return runMainLoop();
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
