#pragma once

#include <optional>

#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::analytics { class Manager; }

namespace nx::vms::server::rest {

class DeviceAnalyticsSettingsHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
    DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule);
    virtual RestResponse executeGet(const RestRequest& request);
    virtual RestResponse executePost(const RestRequest& request);

private:
    std::optional<RestResponse> checkCommonInputParameters(const RestRequest& request) const;
    std::optional<QJsonObject> deviceAgentSettingsModel(const QString& engineId) const;

    RestResponse makeSettingsResponse(
        const nx::vms::server::analytics::Manager* analyticsManager,
        const QString& engineId,
        const QString& deviceId) const;
};

} // namespace nx::vms::server::rest
