#include "rest_handlers.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/media_server_module.h>
#include <network/router.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/timer_manager.h>

namespace nx::vms::server::metrics {

static const QString kRulesApi("/rules");
static const QString kManifestApi("/manifest");
static const QString kValuesApi("/values");
static const QString kAlarmsApi("/alarms");

static const QString kSystemParam("system");
static const QString kFormattedParam("formatted");

LocalRestHandler::LocalRestHandler(utils::metrics::SystemController* controller):
    m_controller(controller)
{
}

JsonRestResponse LocalRestHandler::executeGet(const JsonRestRequest& request)
{
    using utils::metrics::Scope;
    const Scope scope = request.params.contains(kSystemParam) ? Scope::system : Scope::local;

    if (request.path.endsWith(kValuesApi))
        return JsonRestResponse(m_controller->values(scope, request.params.contains(kFormattedParam)));

    if (request.path.endsWith(kAlarmsApi))
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
JsonRestResponse queryAndMerge(Values values, const QString& api, const QString& query, QnMediaServerModule* module);

JsonRestResponse SystemRestHandler::executeGet(const JsonRestRequest& request)
{
    if (request.path.endsWith(kRulesApi))
         return JsonRestResponse(m_controller->rules());

    if (request.path.endsWith(kManifestApi))
        return JsonRestResponse(m_controller->manifest());

    using utils::metrics::Scope;
    if (request.path.endsWith(kValuesApi))
    {
        const auto formatted = request.params.contains(kFormattedParam);
        const auto query = formatted ? kFormattedParam : QString();
        return queryAndMerge(m_controller->values(Scope::system, formatted), kValuesApi, query, serverModule());
    }

    if (request.path.endsWith(kAlarmsApi))
        return queryAndMerge(m_controller->alarms(Scope::system), kAlarmsApi, QString(), serverModule());

    return JsonRestResponse(nx::network::http::StatusCode::notFound, QnJsonRestResult::BadRequest);
}

static QString forLog(const api::metrics::SystemValues& serverValues)
{
    QStringList strings;
    for (const auto& [name, values]: serverValues)
        strings << nx::utils::log::makeMessage("%1 %2").args(values.size(), name);
    return strings.join(", ");
}

static QString forLog(const api::metrics::Alarms& alarms)
{
    return nx::utils::log::makeMessage("%1 alarms", alarms.size());
}

template<typename Values>
JsonRestResponse queryAndMerge(Values values, const QString& api, const QString& query, QnMediaServerModule* module)
{
    static const nx::utils::log::Tag kTag(typeid(SystemRestHandler));
    for (const auto& server: module->commonModule()->resourcePool()->getResources<QnMediaServerResource>())
    {
        if (server->getId() == module->commonModule()->moduleGUID())
            continue;

        const auto route = module->commonModule()->router()->routeTo(server->getId());
        if (route.id.isNull())
        {
            NX_DEBUG(kTag, "No route to %1", server);
            continue;
        }

        const auto url = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName).setEndpoint(route.addr)
            .setPath("/api/metrics" + api).setQuery(query);

        // TODO: Use http::AsyncClient to run these requests in parallel.
        nx::network::http::HttpClient client;
        client.addAdditionalHeader(Qn::EC2_SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
        client.setUserName(server->getId().toString());
        client.setUserPassword(server->getAuthKey());
        if (!client.doGet(url) || !client.response())
        {
            NX_DEBUG(kTag, "Connect to %1 (%2) has failed", server, url);
            continue;
        }

        const auto message = client.fetchMessageBodyBuffer();
        const auto httpCode = client.response()->statusLine.statusCode;
        const auto result = QJson::deserialized<QnJsonRestResult>(message);
        if (!nx::network::http::StatusCode::isSuccessCode(httpCode)
            || result.error != QnJsonRestResult::NoError)
        {
            NX_DEBUG(kTag, "Request to %1 (%2) has failed %3 (%4)", server, url, httpCode, result.errorString);
            continue;
        }

        auto serverValues = result.deserialized<Values>();
        NX_DEBUG(kTag, "%1 (%2) returned %3", server, url, forLog(serverValues));
        api::metrics::merge(&values, &serverValues);
    }

    return JsonRestResponse(values);
}

} // namespace nx::vms::server::metrics
