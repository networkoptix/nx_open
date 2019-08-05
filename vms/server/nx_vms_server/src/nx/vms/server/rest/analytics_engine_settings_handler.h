#pragma once

#include <nx/network/rest/handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server {

class AnalyticsEngineSettingsHandler: public nx::network::rest::Handler, public ServerModuleAware
{
public:
    AnalyticsEngineSettingsHandler(QnMediaServerModule* serverModule);
    virtual nx::network::rest::Response executeGet(const nx::network::rest::Request& request);
    virtual nx::network::rest::Response executePost(const nx::network::rest::Request& request);
};

} // namespace nx::vms::server
