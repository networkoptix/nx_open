#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::rest {

class AnalyticsEngineSettingsHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
    AnalyticsEngineSettingsHandler(QnMediaServerModule* serverModule);
    virtual RestResponse executeGet(const RestRequest& request);
    virtual RestResponse executePost(const RestRequest& request);
};

} // namespace nx::vms::server::rest
