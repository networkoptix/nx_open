#pragma once

#include <map>
#include <optional>

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::analytics { class Manager; }

namespace nx::vms::server::rest {

class DeviceAnalyticsSettingsHandler: public QnJsonRestHandler, public ServerModuleAware
{
    struct CommonRequestEntities
    {
        QnVirtualCameraResourcePtr device;
        nx::vms::server::resource::AnalyticsEngineResourcePtr engine;

        std::optional<JsonRestResponse> errorResponse;
    };

public:
    DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule);
    virtual JsonRestResponse executeGet(const JsonRestRequest& request);
    virtual JsonRestResponse executePost(const JsonRestRequest& request, const QByteArray& body);

    virtual DeviceIdRetriever createCustomDeviceIdRetriever() const override;

private:
    CommonRequestEntities extractCommonRequestEntities(
        const std::map<QString, QString>& parameters) const;

    JsonRestResponse makeSettingsResponse(
        const nx::vms::server::analytics::Manager* analyticsManager,
        const CommonRequestEntities& commonRequestEntities) const;
};

} // namespace nx::vms::server::rest
