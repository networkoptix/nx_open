#include "nvr_network_block_handler.h"

#include <media_server/media_server_module.h>
#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/nvr/types.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/network_block_data.h>

namespace nx::vms::server::rest {

using namespace nx::vms::server::nvr;

INetworkBlockManager* networkBlockManager(QnMediaServerModule* serverModule)
{
    auto nvrService = serverModule->nvrService();
    if (!NX_ASSERT(nvrService))
        return nullptr;

    return nvrService->networkBlockManager();
}

NvrNetworkBlockHandler::NvrNetworkBlockHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

JsonRestResponse NvrNetworkBlockHandler::executeGet(const JsonRestRequest& request)
{
    if (const INetworkBlockManager* const manager = networkBlockManager(serverModule()))
        return manager->state();

    NX_DEBUG(this,
        "Unable to access network block manager, current server doesn't support PoE management");
    return nx::network::http::StatusCode::notImplemented;
}

JsonRestResponse NvrNetworkBlockHandler::executePost(
    const JsonRestRequest& request, const QByteArray& body)
{
    std::vector<nx::vms::api::NetworkPortWithPoweringMode> portPoweringModes;
    if (!QJson::deserialize(body, &portPoweringModes))
        return nx::network::http::StatusCode::badRequest;

    if (INetworkBlockManager* const manager = networkBlockManager(serverModule()))
    {
        if (manager->setPoeModes(portPoweringModes))
            return manager->state();

        return nx::network::http::StatusCode::internalServerError;
    }

    return nx::network::http::StatusCode::notImplemented;
}

} // namespace nx::vms::server::rest
