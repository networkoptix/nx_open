#include "plugin_info_handler.h"

#include <media_server/media_server_module.h>
#include <plugins/plugin_manager.h>

#include <nx/vms/api/data/analytics_data.h>

namespace nx::vms::server::rest {

PluginInfoHandler::PluginInfoHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

JsonRestResponse PluginInfoHandler::executeGet(const JsonRestRequest& request)
{
    using namespace nx::network::http;

    auto pluginManager = serverModule()->pluginManager();
    if (!NX_ASSERT(pluginManager, "Unable to access plugin manager"))
    {
        return JsonRestResponse(
            StatusCode::internalServerError,
            QnRestResult::Error::InternalServerError);
    }

    return JsonRestResponse(pluginManager->pluginInformation());
}

} // namespace nx::vms::server::rest
