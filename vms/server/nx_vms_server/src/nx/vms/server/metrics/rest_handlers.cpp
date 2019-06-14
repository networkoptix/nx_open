#include "rest_handlers.h"

#include <core/resource/media_server_resource.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/timer_manager.h>

namespace nx::vms::server::metrics {

using namespace nx::network;

// TODO: Move to nx::utils?
static std::optional<QJsonValue> cleanupJson(QJsonValue value)
{
    switch (value.type())
    {
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

LocalRestHandler::LocalRestHandler(utils::metrics::Controller* controller):
    m_controller(controller)
{
}

rest::Response LocalRestHandler::executeGet(const rest::Request& request)
{
    if (request.path().endsWith("/rules"))
        return rest::Response::reply(cleanJson(m_controller->rules()));

    const auto applyRules = !request.param("noRules");
    if (request.path().endsWith("/manifest"))
        return rest::Response::reply(cleanJson(m_controller->manifest(applyRules)));

    std::optional<std::chrono::milliseconds> timeLine;
    if (const auto value = request.param("timeLine"))
        timeLine = nx::utils::parseTimerDuration(*value, std::chrono::hours(24));

    if (request.path().endsWith("/values"))
        return rest::Response::reply(cleanJson(m_controller->values(applyRules, timeLine)));

    return rest::Response::error(http::StatusCode::notFound, rest::Result::BadRequest);
}

SystemRestHandler::SystemRestHandler(
    utils::metrics::Controller* controller, QnResourcePool* resourcePool)
:
    LocalRestHandler(controller),
    m_resourcePool(resourcePool)
{
}

rest::Response SystemRestHandler::executeGet(const rest::Request& request)
{
    if (!request.path().endsWith("/values"))
        return LocalRestHandler::executeGet(request);

    api::metrics::SystemValues systemValues;
    for (const auto& server: m_resourcePool->getResources<QnMediaServerResource>())
    {
        http::HttpClient client;
        client.setUserName(server->getId().toString());
        client.setUserPassword(server->getAuthKey());

        nx::utils::Url url(server->getUrl());
        url.setPath("/api/metrics/values");
        url.setQuery(request.params().toUrlQuery());
        if (!client.doGet(url) || !client.response())
        {
            NX_DEBUG(this, "Query [ %1 ] has failed", url);
            continue;
        }

        http::BufferType msgBody;
        while (!client.eof())
            msgBody.append(client.fetchMessageBodyBuffer());

        const auto httpCode = client.response()->statusLine.statusCode;
        const auto result = QJson::deserialized<rest::JsonResult>(msgBody);
        if (!http::StatusCode::isSuccessCode(httpCode)
            || result.error != rest::Result::Error::NoError)
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
    return rest::Response::reply(cleanJson(systemValues));
}

} // namespace nx::vms::server::metrics
