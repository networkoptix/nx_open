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
    virtual JsonRestResponse executeGet(const JsonRestRequest& request);
    virtual JsonRestResponse executePost(const JsonRestRequest& request, const QByteArray& body);

private:
    std::optional<JsonRestResponse> checkCommonInputParameters(
        const QnRequestParams& parameters) const;

    std::optional<QJsonObject> deviceAgentSettingsModel(const QString& engineId) const;

    JsonRestResponse makeSettingsResponse(
        const nx::vms::server::analytics::Manager* analyticsManager,
        const QString& engineId,
        const QString& deviceId) const;
};

} // namespace nx::vms::server::rest
