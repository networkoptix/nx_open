#include "nvr_network_block_handler.h"

#include <media_server/media_server_module.h>
#include <nx/vms/server/nvr/i_service.h>

namespace nx::vms::server::rest {

using namespace nx::vms::server::nvr;

INetworkBlockController* networkBlockController(QnMediaServerModule* serverModule)
{
    auto nvrService = serverModule->nvrService();
    if (!nvrService)
        return nullptr;

    return nvrService->networkBlockController();
}

NvrNetworkBlockHandler::NvrNetworkBlockHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

JsonRestResponse NvrNetworkBlockHandler::executeGet(const JsonRestRequest& request)
{
    if (const INetworkBlockController* const manager = networkBlockController(serverModule()))
        return manager->state();

    return nx::network::http::StatusCode::notImplemented;
}

JsonRestResponse NvrNetworkBlockHandler::executePost(
    const JsonRestRequest& request, const QByteArray& body)
{
    INetworkBlockController::PoweringModeByPort poweringModeByPort;
    if (!QJson::deserialize(body, &poweringModeByPort))
        return nx::network::http::StatusCode::badRequest;

    if (INetworkBlockController* const manager = networkBlockController(serverModule()))
    {
        if (manager->setPortPoweringModes(poweringModeByPort))
            return manager->state();

        return nx::network::http::StatusCode::internalServerError;
    }

    return nx::network::http::StatusCode::notImplemented;
}

} // namespace nx::vms::server::rest
