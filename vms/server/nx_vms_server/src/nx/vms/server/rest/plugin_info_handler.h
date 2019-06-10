#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::rest {

class PluginInfoHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
    PluginInfoHandler(QnMediaServerModule* serverModule);
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;
};

} // namespace nx::vms::server::rest
