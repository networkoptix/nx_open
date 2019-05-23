#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::rest {

class PluginModuleInformationHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
    PluginModuleInformationHandler(QnMediaServerModule* serverModule);
    virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;
};

} // namespace nx::vms::server::rest
