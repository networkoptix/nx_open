#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::rest {

class PluginInfoHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
    PluginInfoHandler(QnMediaServerModule* serverModule);
    virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;
};

} // namespace nx::vms::server::rest
