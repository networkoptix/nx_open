#include "device_analytics_settings_handler.h"

#include <variant>

#include <core/resource_management/resource_pool.h>
#include <api/helpers/camera_id_helper.h>
#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/vms/server/rest/parameter_names.h>
#include <nx/vms/server/rest/utils.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/analytics/manager.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/resource/camera.h>

#include <nx/vms/api/analytics/settings_response.h>
#include <nx/vms/api/analytics/device_analytics_settings_data.h>

namespace nx::vms::server::rest {

using namespace nx::network;
using DeviceAnalyticsSettingsRequest = nx::vms::api::analytics::DeviceAnalyticsSettingsRequest;
using DeviceAnalyticsSettingsResponse = nx::vms::api::analytics::DeviceAnalyticsSettingsResponse;
using DeviceAgentManfiest = nx::vms::api::analytics::DeviceAgentManifest;

using ParameterMap = std::map<QString, QString>;

template<typename Data>
using DataOrError = std::variant<Data, /*errorResponse*/ JsonRestResponse>;
using CommonRequestParametersOrError = DataOrError<ParameterMap>;

CommonRequestParametersOrError extractCommonRequestParametersFromBody(
    const QByteArray& body)
{
    bool success = false;
    if (const auto settingsRequest =
        QJson::deserialized(body, DeviceAnalyticsSettingsRequest(), &success);
        success)
    {
        ParameterMap result;

        if (!settingsRequest.deviceId.isNull())
            result[kDeviceIdParameter] = settingsRequest.deviceId;

        if (!settingsRequest.analyticsEngineId.isNull())
            result[kAnalyticsEngineIdParameter] = settingsRequest.analyticsEngineId.toString();

        return result;
    }

    return makeResponse(QnRestResult::CantProcessRequest, "Unable to deserialize request body");
}

ParameterMap extractCommonRequestParametersFromUrlQuery(
    const JsonRestRequest& request)
{
    ParameterMap result;

    const QnRequestParams& requestQueryParameters = request.params;
    if (requestQueryParameters.contains(kDeviceIdParameter))
        result[kDeviceIdParameter] = requestQueryParameters[kDeviceIdParameter];

    if (requestQueryParameters.contains(kAnalyticsEngineIdParameter))
        result[kAnalyticsEngineIdParameter] = requestQueryParameters[kAnalyticsEngineIdParameter];

    return result;
}

DeviceAnalyticsSettingsHandler::DeviceAnalyticsSettingsHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

JsonRestResponse DeviceAnalyticsSettingsHandler::executeGet(const JsonRestRequest& request)
{
    const ParameterMap commonRequestParameters =
        extractCommonRequestParametersFromUrlQuery(request);

    const CommonRequestEntities commonRequestEntities =
        extractCommonRequestEntities(commonRequestParameters);

    if (commonRequestEntities.errorResponse)
        return *commonRequestEntities.errorResponse;

    const auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return makeResponse(QnRestResult::InternalServerError, message);
    }

    return makeSettingsResponse(analyticsManager, commonRequestEntities);
}

JsonRestResponse DeviceAnalyticsSettingsHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    const CommonRequestParametersOrError commonRequestParametersOrError =
        extractCommonRequestParametersFromBody(body);

    if (!NX_ASSERT(!commonRequestParametersOrError.valueless_by_exception()))
        return makeResponse(QnRestResult::InternalServerError, "Internal server error");

    if (std::holds_alternative<JsonRestResponse>(commonRequestParametersOrError))
        return std::get<JsonRestResponse>(commonRequestParametersOrError);

    const auto commonRequestParameters = std::get<ParameterMap>(commonRequestParametersOrError);

    const CommonRequestEntities commonRequestEntities =
        extractCommonRequestEntities(commonRequestParameters);

    if (commonRequestEntities.errorResponse)
        return *commonRequestEntities.errorResponse;

    bool success = false;
    const auto& settingsRequest =
        QJson::deserialized(body, DeviceAnalyticsSettingsRequest(), &success);

    if (!success)
    {
        const QString message("Unable to deserialize request");
        NX_WARNING(this, message);
        return makeResponse(QnRestResult::Error::BadRequest, message);
    }

    auto analyticsManager = serverModule()->analyticsManager();
    if (!analyticsManager)
    {
        const QString message("Unable to access analytics manager");
        NX_ERROR(this, message);
        return makeResponse(QnRestResult::InternalServerError, message);
    }

    if (settingsRequest.analyzedStreamIndex != nx::vms::api::StreamIndex::undefined)
    {
        commonRequestEntities.device->setAnalyzedStreamIndex(
            commonRequestEntities.engine->getId(),
            settingsRequest.analyzedStreamIndex);

        commonRequestEntities.device->saveProperties();
    }

    analyticsManager->setSettings(
        commonRequestParameters.at(kDeviceIdParameter),
        commonRequestParameters.at(kAnalyticsEngineIdParameter),
        settingsRequest.settingsValues);

    return makeSettingsResponse(analyticsManager, commonRequestEntities);
}

DeviceAnalyticsSettingsHandler::CommonRequestEntities
    DeviceAnalyticsSettingsHandler::extractCommonRequestEntities(
        const ParameterMap& parameters) const
{
    CommonRequestEntities result;
    const auto deviceIdIt = parameters.find(kDeviceIdParameter);
    if (deviceIdIt == parameters.cend() || deviceIdIt->second.isEmpty())
    {
        NX_WARNING(this, "Missing parameter %1", kDeviceIdParameter);
        result.errorResponse = makeResponse(
            QnRestResult::Error::MissingParameter,
            QStringList{kDeviceIdParameter});

        return result;
    }

    const auto analyticsEngineIdIt = parameters.find(kAnalyticsEngineIdParameter);
    if (analyticsEngineIdIt == parameters.cend() || analyticsEngineIdIt->second.isEmpty())
    {
        NX_WARNING(this, "Missing parameter %1", kAnalyticsEngineIdParameter);
        result.errorResponse = makeResponse(
            QnRestResult::Error::MissingParameter,
            QStringList{kAnalyticsEngineIdParameter});

        return result;
    }

    const QString& deviceId = deviceIdIt->second;
    const QString& engineId = analyticsEngineIdIt->second;

    const auto device = nx::camera_id_helper::findCameraByFlexibleId(
        serverModule()->resourcePool(),
        deviceId);

    if (!device)
    {
        const auto message = lm("Unable to find device by id %1").args(deviceId);
        NX_WARNING(this, message);
        result.errorResponse = makeResponse(QnRestResult::Error::CantProcessRequest, message);

        return result;
    }

    if (device->flags().testFlag(Qn::foreigner))
    {
        const auto message =
            lm("Wrong server. Device belongs to the server %1, current server: %2").args(
                device->getParentId(), moduleGUID());

        NX_WARNING(this, message);
        result.errorResponse = makeResponse(QnRestResult::Error::CantProcessRequest, message);

        return result;
    }

    const auto engine = sdk_support::find<resource::AnalyticsEngineResource>(
        serverModule(), engineId);

    if (!engine)
    {
        const auto message = lm("Unable to find analytics engine by id %1").args(engineId);
        NX_WARNING(this, message);
        result.errorResponse = makeResponse(QnRestResult::Error::CantProcessRequest, message);

        return result;
    }

    result.device = device;
    result.engine = engine;

    return result;
}

JsonRestResponse DeviceAnalyticsSettingsHandler::makeSettingsResponse(
    const nx::vms::server::analytics::Manager* analyticsManager,
    const CommonRequestEntities& commonRequestEntities) const
{
    NX_ASSERT(analyticsManager);
    if (!analyticsManager)
    {
        return makeResponse(
            QnRestResult::InternalServerError,
            "Unable to access an analytics manager");
    }

    JsonRestResponse result(http::StatusCode::ok);
    nx::vms::api::analytics::DeviceAnalyticsSettingsResponse response;

    const QnUuid deviceId = commonRequestEntities.device->getId();
    const QnUuid engineId = commonRequestEntities.engine->getId();

    const std::optional<analytics::Settings>& settings = analyticsManager->getSettings(
        deviceId.toString(), engineId.toString());

    response.analyzedStreamIndex =
        commonRequestEntities.device->analyzedStreamIndex(engineId);

    const std::optional<DeviceAgentManfiest> deviceAgentManifest =
        commonRequestEntities.device->deviceAgentManifest(engineId);

    if (deviceAgentManifest)
    {
        response.hideStreamSelection =
            deviceAgentManifest->capabilities.testFlag(
                DeviceAgentManfiest::Capability::hideStreamSelection);
    }

    if (settings)
    {
        response.settingsModel = settings->model;
        response.settingsValues = settings->values;
    }
    else
    {
        const auto message =
            lm("Unable to load DeviceAgent settings for the Engine with id %1").args(engineId);
        return makeResponse(QnRestResult::Error::CantProcessRequest, message);
    }

    result.json.setReply(response);

    return result;
}

DeviceIdRetriever DeviceAnalyticsSettingsHandler::createCustomDeviceIdRetriever() const
{
    return
        [](const nx::network::http::Request& request)
        {
            const QUrlQuery urlQuery(request.requestLine.url.query());
            QString deviceId = urlQuery.queryItemValue(kDeviceIdParameter);

            if (!deviceId.isEmpty())
                return deviceId;

            const CommonRequestParametersOrError commonParametersOrError =
                extractCommonRequestParametersFromBody(request.messageBody);

            if (!std::holds_alternative<ParameterMap>(commonParametersOrError))
                return QString();

            const auto commonParameters =
                std::get<ParameterMap>(commonParametersOrError);

            if (const auto it = commonParameters.find(kDeviceIdParameter);
                it != commonParameters.cend())
            {
                return it->second;
            }

            return QString();
        };
}

} // namespace nx::vms::server::rest
