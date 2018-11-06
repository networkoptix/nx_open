#pragma once

#include <optional>

#include <rest/server/json_rest_handler.h>
#include <nx/mediaserver/server_module_aware.h>

namespace nx::mediaserver::rest {

class DeviceAnalyticsSettingsHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
    DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule);
    virtual JsonRestResponse executeGet(const JsonRestRequest& request);
    virtual JsonRestResponse executePost(const JsonRestRequest& request, const QByteArray& body);

private:
    std::optional<JsonRestResponse> checkCommonInputParameters(
        const QnRequestParams& parameters) const;
};

} // namespace nx::mediaserver::rest
