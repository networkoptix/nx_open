#pragma once

#include <optional>

#include <nx/network/rest/handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::analytics { class Manager; }

namespace nx::vms::server {

class DeviceAnalyticsSettingsHandler: public nx::network::rest::Handler, public ServerModuleAware
{
public:
    DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule);
    virtual nx::network::rest::Response executeGet(const nx::network::rest::Request& request);
    virtual nx::network::rest::Response executePost(const nx::network::rest::Request& request);

private:
    std::optional<nx::network::rest::Response> checkCommonInputParameters(
        const nx::network::rest::Request& request) const;

    std::optional<QJsonObject> deviceAgentSettingsModel(const QString& engineId) const;

    nx::network::rest::Response makeSettingsResponse(
        const nx::vms::server::analytics::Manager* analyticsManager,
        const QString& engineId,
        const QString& deviceId) const;
};

} // namespace nx::vms::server
