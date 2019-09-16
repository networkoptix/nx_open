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
    auto scope = utils::metrics::Scope::local;
    if (request.params.contains("system"))
        scope = utils::metrics::Scope::system;

    if (request.path.endsWith("/values"))
        return JsonRestResponse(m_controller->values(scope));

    if (request.path.endsWith("/alarms"))
        return JsonRestResponse(m_controller->alarms(scope));

    return JsonRestResponse(nx::network::http::StatusCode::notFound, QnJsonRestResult::BadRequest);
}

SystemRestHandler::SystemRestHandler(
    utils::metrics::SystemController* controller, QnMediaServerModule* serverModule)
:
    LocalRestHandler(controller),
    ServerModuleAware(serverModule)
{
}

template<typename Values>
JsonRestResponse queryAndMerge(
    Values values, const QString& api,
    const QnSharedResourcePointerList<QnMediaServerResource>& servers);

JsonRestResponse SystemRestHandler::executeGet(const JsonRestRequest& request)
{
    if (request.path.endsWith("/rules"))
         return JsonRestResponse(m_controller->rules());

    if (request.path.endsWith("/manifest"))
        return JsonRestResponse(m_controller->manifest());

    QnSharedResourcePointerList<QnMediaServerResource> otherServers;
    for (auto& s: serverModule()->commonModule()->resourcePool()->getResources<QnMediaServerResource>())
    {
        if (s->getId() != moduleGUID())
            otherServers.push_back(std::move(s));
    }

    if (request.path.endsWith("/values"))
        return queryAndMerge(m_controller->values(utils::metrics::Scope::system), "values", otherServers);

    if (request.path.endsWith("/alarms"))
        return queryAndMerge(m_controller->alarms(utils::metrics::Scope::system), "alarms", otherServers);

    return JsonRestResponse(nx::network::http::StatusCode::notFound, QnJsonRestResult::BadRequest);
}

static void logResponse(const nx::utils::Url& url, const api::metrics::SystemValues& serverValues)
{
    for (const auto& [name, values]: serverValues)
        NX_DEBUG(typeid(SystemRestHandler), "Query [ %1 ] returned %2 %3", url, values.size(), name);
}

static void logResponse(const nx::utils::Url& url, const api::metrics::Alarms& alarms)
{
    NX_DEBUG(typeid(SystemRestHandler), "Query [ %1 ] returned %2 alarms", url, alarms.size());
}

template<typename Values>
JsonRestResponse queryAndMerge(
    Values values, const QString& api,
    const QnSharedResourcePointerList<QnMediaServerResource>& servers)
{
    static const nx::utils::log::Tag kTag(typeid(SystemRestHandler));
    for (const auto& server: servers)
    {
        nx::network::http::HttpClient client;
        client.setUserName(server->getId().toString());
        client.setUserPassword(server->getAuthKey());

        nx::utils::Url url(server->getUrl());
        url.setPath("/api/metrics/" + api);
        if (!client.doGet(url) || !client.response())
        {
            NX_DEBUG(kTag, "Query [ %1 ] has failed", url);
            continue;
        }

        const auto message = client.fetchMessageBodyBuffer();
        const auto httpCode = client.response()->statusLine.statusCode;
        const auto result = QJson::deserialized<QnJsonRestResult>(message);
        if (!nx::network::http::StatusCode::isSuccessCode(httpCode)
            || result.error != QnJsonRestResult::NoError)
        {
            NX_DEBUG(kTag, "Query [ %1 ] has failed %1 (%2)", url, httpCode, result.errorString);
            continue;
        }

        auto serverValues = result.deserialized<Values>();
        logResponse(url, serverValues);
        api::metrics::merge(&values, &serverValues);
    }

    return JsonRestResponse(values);
}

} // namespace nx::vms::server::metrics
