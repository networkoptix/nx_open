#include "rest_handlers.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/media_server_module.h>
#include <network/router.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/timer_manager.h>
#include <nx/vms/network/abstract_server_connector.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_access/resource_access_manager.h>

namespace nx::vms::server::metrics {

static const QString kRulesApi("/rules");
static const QString kManifestApi("/manifest");
static const QString kValuesApi("/values");
static const QString kAlarmsApi("/alarms");

static const QString kSystemParam("system");
static const QString kFormattedParam("formatted");

static constexpr std::chrono::seconds kDefaultConnectTimeout(5);

static bool hasAdminPermission(const QnRestConnectionProcessor* processor)
{
    return processor->commonModule()->resourceAccessManager()->hasGlobalPermission(
        processor->accessRights(), GlobalPermission::admin);
}

LocalRestHandler::LocalRestHandler(utils::metrics::SystemController* controller):
    m_controller(controller)
{
}

JsonRestResponse LocalRestHandler::executeGet(const JsonRestRequest& request)
{
    if (!hasAdminPermission(request.owner))
        return JsonRestResponse(nx::network::http::StatusCode::forbidden, QnJsonRestResult::Forbidden);

    using utils::metrics::Scope;
    const Scope scope = request.params.contains(kSystemParam) ? Scope::system : Scope::local;

    if (request.path.endsWith(kValuesApi))
        return JsonRestResponse(m_controller->values(scope, request.params.contains(kFormattedParam)));

    if (request.path.endsWith(kAlarmsApi))
        return JsonRestResponse(m_controller->alarms(scope));

    return JsonRestResponse(nx::network::http::StatusCode::notFound, QnJsonRestResult::BadRequest);
}

SystemRestHandler::SystemRestHandler(
    utils::metrics::SystemController* controller,
    QnMediaServerModule* serverModule,
    nx::vms::network::AbstractServerConnector* serverConnector)
:
    LocalRestHandler(controller),
    ServerModuleAware(serverModule),
    m_serverConnector(serverConnector)
{
}

JsonRestResponse SystemRestHandler::executeGet(const JsonRestRequest& request)
{
    if (!hasAdminPermission(request.owner))
        return JsonRestResponse(nx::network::http::StatusCode::forbidden, QnJsonRestResult::Forbidden);

    if (request.path.endsWith(kRulesApi))
         return JsonRestResponse(m_controller->rules());

    if (request.path.endsWith(kManifestApi))
        return JsonRestResponse(m_controller->manifest());

    using utils::metrics::Scope;
    if (request.path.endsWith(kValuesApi))
    {
        const auto formatted = request.params.contains(kFormattedParam);
        const auto query = formatted ? kFormattedParam : QString();
        return getAndMerge(m_controller->values(Scope::system, formatted), kValuesApi, query);
    }

    if (request.path.endsWith(kAlarmsApi))
        return getAndMerge(m_controller->alarms(Scope::system), kAlarmsApi);

    return JsonRestResponse(nx::network::http::StatusCode::notFound, QnJsonRestResult::BadRequest);
}

template<typename Values>
QString forLog(const Values& serverValues)
{
    QStringList strings;
    for (const auto& [name, values]: serverValues)
        strings << nx::utils::log::makeMessage("%1 %2").args(values.size(), name);
    return strings.join(", ");
}

template<typename Values>
JsonRestResponse SystemRestHandler::getAndMerge(
    Values values, const QString& api, const QString& query)
{
    const auto common = serverModule()->commonModule();
    std::vector<cf::future<Values>> serverFutures;
    for (const auto& server: common->resourcePool()->getResources<QnMediaServerResource>())
    {
        const auto serverId = server->getId();
        if (serverId == common->moduleGUID())
            continue;

        if (server->getStatus() == Qn::Offline)
        {
            NX_VERBOSE(this, "Skip offline server %1", serverId);
            continue;
        }

        const auto url = nx::network::url::Builder()
            .setHost("no-host") //< Prevent assert in http::AsyncClient.
            .setUserName(serverId.toString())
            .setPassword(server->getAuthKey())
            .setPath("/api/metrics" + api)
            .setQuery(query);

        serverFutures.push_back(getAsync(serverId, url).then(
            [this, serverId, url](auto result)
            {
                Values values;
                QJson::deserialize<Values>(result.get(), &values);
                NX_DEBUG(this, "Server %1 (%2) returned %3", serverId, url, forLog(values));
                return values;
            }));
    }

    for (auto& future: serverFutures)
    {
        auto serverValues = future.get();
        api::metrics::merge(&values, &serverValues);
    }

    return JsonRestResponse(values);
}

cf::future<QJsonValue> SystemRestHandler::getAsync(const QnUuid& server, const nx::utils::Url& url)
{
    return m_serverConnector->connect(server, kDefaultConnectTimeout).then(
        [this, server, url](auto result) mutable
        {
            auto connection = result.get();
            if (!connection)
            {
                NX_DEBUG(this, "Connect to %1 has failed", server);
                return cf::make_ready_future(QJsonValue());
            }

            cf::promise<QJsonValue> promise;
            auto future = promise.get_future();
            auto client = std::make_unique<nx::network::http::AsyncClient>(std::move(connection));
            client->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, server.toByteArray());
            client.get()->doGet(
                url,
                [this, url = std::move(url), server = std::move(server),
                    client = std::move(client), promise = std::move(promise)]() mutable
                {
                    if (!client->response())
                    {
                        NX_DEBUG(this, "Request to %1 (%2) has failed: %3",
                            server, url, SystemError::toString(client->lastSysErrorCode()));
                        return promise.set_value(QJsonValue());
                    }

                    const auto httpCode = client->response()->statusLine.statusCode;
                    const auto message = client->fetchMessageBodyBuffer();
                    const auto result = QJson::deserialized<QnJsonRestResult>(message);
                    if (!nx::network::http::StatusCode::isSuccessCode(httpCode)
                        || result.error != QnJsonRestResult::NoError)
                    {
                        NX_DEBUG(this, "Request to %1 (%2) has failed with code %3: %4",
                            server, url, httpCode, result.errorString);
                        return promise.set_value(QJsonValue());
                    }

                    promise.set_value(result.reply);
                });

            return future;
        });
}

} // namespace nx::vms::server::metrics
