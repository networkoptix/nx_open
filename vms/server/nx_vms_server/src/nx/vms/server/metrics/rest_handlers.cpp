#include "rest_handlers.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/timer_manager.h>

namespace nx::vms::server::metrics {

// TODO: Move to nx::utils?
static std::optional<QJsonValue> cleanupJson(QJsonValue value)
{
    switch (value.type())
    {
        case QJsonValue::Null:
            return std::nullopt;

        case QJsonValue::String:
        {
            if (value.toString().isEmpty())
                return std::nullopt;
            else
                return value;
        }

        case QJsonValue::Array:
        {
            const auto originalArray = value.toArray();
            if (originalArray.isEmpty())
                return std::nullopt;

            QJsonArray newArray;
            for (auto it = originalArray.begin(); it != originalArray.end(); ++it)
            {
                if (auto value = cleanupJson(*it))
                    newArray.push_back(*value);
            }

            return newArray;
        }

        case QJsonValue::Object:
        {
            const auto originalObject = value.toObject();
            if (originalObject.isEmpty())
                return std::nullopt;

            QJsonObject newObject;
            for (auto it = originalObject.begin(); it != originalObject.end(); ++it)
            {
                if (auto value = cleanupJson(it.value()))
                    newObject[it.key()] = std::move(*value);
            }

            return newObject;
        }

        default:
            return value;
    };
}

template<typename T>
QJsonValue cleanJson(const T& value)
{
    QJsonValue json;
    QJson::serialize(value, &json);

    const auto cleanJson = cleanupJson(json);
    return cleanJson ? *cleanJson : QJsonValue();
}

using Controller = utils::metrics::Controller;
struct Parameters
{
    Controller::RequestFlags flags;
    std::optional<std::chrono::milliseconds> timeline;

    Parameters(const JsonRestRequest& request):
        flags(request.params.contains("noRules")
            ? Controller::RequestFlag::none
            : Controller::RequestFlag::applyRules)
    {
        if (request.params.contains("timeline"))
            timeline = nx::utils::parseTimerDuration(request.params.value("timeline"));
    }
};

LocalRestHandler::LocalRestHandler(utils::metrics::Controller* controller):
    m_controller(controller)
{
}

JsonRestResponse LocalRestHandler::executeGet(const JsonRestRequest& request)
{
    if (request.path.endsWith("/rules"))
        return JsonRestResponse(cleanJson(m_controller->rules()));

    Parameters params(request);
    if (request.path.endsWith("/manifest"))
        return JsonRestResponse(cleanJson(m_controller->manifest(params.flags)));

    if (request.path.endsWith("/values"))
        return JsonRestResponse(cleanJson(m_controller->values(params.flags, params.timeline)));

    return JsonRestResponse(nx::network::http::StatusCode::notFound, QnJsonRestResult::BadRequest);
}

SystemRestHandler::SystemRestHandler(
    utils::metrics::Controller* controller, QnMediaServerModule* serverModule)
:
    LocalRestHandler(controller),
    ServerModuleAware(serverModule)
{
}

JsonRestResponse SystemRestHandler::executeGet(const JsonRestRequest& request)
{
    if (!request.path.endsWith("/values"))
        return LocalRestHandler::executeGet(request);

    Parameters params(request);
    auto systemValues = m_controller->values(
        params.flags | Controller::includeRemote, params.timeline);

    for (const auto& server: serverModule()->commonModule()
        ->resourcePool()->getResources<QnMediaServerResource>())
    {
        if (server->getId() == moduleGUID())
            continue;

        nx::network::http::HttpClient client;
        client.setUserName(server->getId().toString());
        client.setUserPassword(server->getAuthKey());

        QUrlQuery query;
        for (auto it = request.params.begin(); it != request.params.end(); ++it)
            query.addQueryItem(it.key(), it.value());

        nx::utils::Url url(server->getUrl());
        url.setPath("/api/metrics/values");
        url.setQuery(query);
        if (!client.doGet(url) || !client.response())
        {
            NX_DEBUG(this, "Query [ %1 ] has failed", url);
            continue;
        }

        nx::network::http::BufferType msgBody;
        while (!client.eof())
            msgBody.append(client.fetchMessageBodyBuffer());

        const auto httpCode = client.response()->statusLine.statusCode;
        const auto result = QJson::deserialized<QnJsonRestResult>(msgBody);
        if (!nx::network::http::StatusCode::isSuccessCode(httpCode)
            || result.error != QnJsonRestResult::NoError)
        {
            NX_DEBUG(this, "Query [ %1 ] has failed %1 (%2)", url, httpCode, result.errorString);
            continue;
        }

        auto serverValues = result.deserialized<api::metrics::SystemValues>();
        for (const auto& [name, values]: serverValues)
            NX_VERBOSE(this, "Query [ %1 ] returned %2 %3", url, values.size(), name);

        api::metrics::merge(&systemValues, &serverValues);
    }

    // TODO: Add some system specific information.
    return JsonRestResponse(cleanJson(systemValues));
}

} // namespace nx::vms::server::metrics
