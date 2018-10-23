#pragma once

#include <type_traits>

#include <core/resource/resource_fwd.h>
#include <rest/server/json_rest_handler.h>

#include <nx/mediaserver/resource/resource_fwd.h>
#include <nx/mediaserver/server_module_aware.h>

#include <nx/utils/std/optional.h>
#include <nx/utils/log/log.h>

namespace nx::mediaserver::rest {

class DeviceAnalyticsSettingsHandler:
    public QnJsonRestHandler,
    public nx::mediaserver::ServerModuleAware
{
public:
    DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule);
    virtual JsonRestResponse executeGet(const JsonRestRequest& request);
    virtual JsonRestResponse executePost(const JsonRestRequest& request, const QByteArray& body);

private:
    template<typename T>
    std::optional<JsonRestResponse> checkCommonInputParameters(const T& parameters) const
    {
        if (!parameters.contains(kDeviceIdParameter))
        {
            NX_WARNING(this, "Missing parameter %1", kDeviceIdParameter);
            return makeResponse(
                QnRestResult::Error::MissingParameter,
                QStringList{kDeviceIdParameter});
        }

        if (!parameters.contains(kAnalyticsEngineIdParameter))
        {
            NX_WARNING(this, "Missing parameter %1", kAnalyticsEngineIdParameter);
            return makeResponse(
                QnRestResult::Error::MissingParameter,
                QStringList{kAnalyticsEngineIdParameter});
        }

        constexpr bool isQVariant =
            std::is_same_v<
                std::remove_cv_t<
                std::remove_reference_t<T::mapped_type>>,
            QVariant>;

        QString deviceId;
        QString engineId;
        if constexpr (isQVariant)
        {
            deviceId = parameters[kDeviceIdParameter].toString();
            engineId = parameters[kAnalyticsEngineIdParameter].toString();
        }
        else
        {
            deviceId = parameters[kDeviceIdParameter];
            engineId = parameters[kAnalyticsEngineIdParameter];
        }

        const auto device = nx::camera_id_helper::findCameraByFlexibleId(
            serverModule()->resourcePool(),
            deviceId);

        if (!device)
        {
            const auto message = lm("Unable to find device by id %1").args(deviceId);
            NX_WARNING(this, message);
            return makeResponse(QnRestResult::Error::CantProcessRequest, message);
        }

        if (device->flags().testFlag(Qn::foreigner))
        {
            const auto message =
                lm("Wrong server. Device belongs to the server %1, current server: %2")
                .args(device->getParentId(), moduleGUID());

            NX_WARNING(this, message);
            return makeResponse(QnRestResult::Error::CantProcessRequest, message);
        }

        const auto engine = sdk_support::find<resource::AnalyticsEngineResource>(
            serverModule(), engineId);

        if (!engine)
        {
            const auto message = lm("Unable to find analytics engine by id %1").args(engineId);
            NX_WARNING(this, message);
            return makeResponse(QnRestResult::Error::CantProcessRequest, message);
        }

        return std::nullopt;
    }
};

} // namespace nx::mediaserver::rest
