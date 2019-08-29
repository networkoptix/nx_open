#include "rest_handlers.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/timer_manager.h>

namespace nx::vms::server::metrics {

LocalRestHandler::LocalRestHandler(utils::metrics::SystemController* controller):
    m_controller(controller)
{
}

JsonRestResponse LocalRestHandler::executeGet(const JsonRestRequest& request)
{
    if (request.path.endsWith("/rules"))
         return JsonRestResponse(m_controller->rules());

    if (request.path.endsWith("/manifest"))
        return JsonRestResponse(m_controller->manifest());

    if (request.path.endsWith("/values"))
        return JsonRestResponse(m_controller->values());

    if (request.path.endsWith("/alarms"))
        return JsonRestResponse(m_controller->alarms());

    return JsonRestResponse(nx::network::http::StatusCode::notFound, QnJsonRestResult::BadRequest);
}

SystemRestHandler::SystemRestHandler(
    utils::metrics::SystemController* controller, QnMediaServerModule* serverModule)
:
    LocalRestHandler(controller),
    ServerModuleAware(serverModule)
{
}

JsonRestResponse SystemRestHandler::executeGet(const JsonRestRequest& request)
{
    if (!request.path.endsWith("/values"))
        return LocalRestHandler::executeGet(request);

    auto systemValues = m_controller->values();
    const auto servers = serverModule()->commonModule()->resourcePool()->getResources<QnMediaServerResource>();
    for (const auto& server: servers)
    {
        if (server->getId() == moduleGUID())
            continue;

        nx::network::http::HttpClient client;
        client.setUserName(server->getId().toString());
        client.setUserPassword(server->getAuthKey());

        nx::utils::Url url(server->getUrl());
        url.setPath("/api/metrics/values");
        if (!client.doGet(url) || !client.response())
        {
            NX_DEBUG(this, "Query [ %1 ] has failed", url);
            continue;
        }

        const auto message = client.fetchMessageBodyBuffer();
        const auto httpCode = client.response()->statusLine.statusCode;
        const auto result = QJson::deserialized<QnJsonRestResult>(message);
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
    return JsonRestResponse(systemValues);
}

} // namespace nx::vms::server::metrics
