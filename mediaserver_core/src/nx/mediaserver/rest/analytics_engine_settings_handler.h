#pragma once

#include <core/resource/resource_fwd.h>
#include <rest/server/json_rest_handler.h>

#include <nx/mediaserver/resource/resource_fwd.h>
#include <nx/mediaserver/server_module_aware.h>

namespace nx::mediaserver::rest {

class AnalyticsEngineSettingsHandler:
    public QnJsonRestHandler,
    public nx::mediaserver::ServerModuleAware
{
public:
    AnalyticsEngineSettingsHandler(QnMediaServerModule* serverModule);
    virtual JsonRestResponse executeGet(const JsonRestRequest& request);
    virtual JsonRestResponse executePost(const JsonRestRequest& request, const QByteArray& body);
};

} // namespace nx::mediaserver::rest
