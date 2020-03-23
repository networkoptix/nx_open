#include "multiserver_plugin_info_handler.h"

#include <optional>

#include <network/tcp_listener.h>
#include <media_server/media_server_module.h>
#include <common/common_module.h>
#include <plugins/plugin_manager.h>

#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/request_helpers.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <nx/utils/scope_guard.h>

#include <nx/vms/api/data/analytics_data.h>

namespace nx::vms::server::rest {

using namespace nx::vms::api;
using namespace nx::network::http;

using ExtendedPluginInfoByServer = std::map<QnUuid, nx::vms::api::PluginInfoExList>;

static const QString kIsLocalParameterName("isLocal");
static const QString kTrue("true");

const QString MultiserverPluginInfoHandler::kPath("ec2/pluginInfo");

static nx::utils::Url buildRemoteRequestUrl(const QnMediaServerResourcePtr& server)
{
    nx::utils::Url apiUrl(server->getApiUrl());
    apiUrl.setPath("/" + MultiserverPluginInfoHandler::kPath);

    QUrlQuery query;
    query.addQueryItem(kIsLocalParameterName, kTrue);
    apiUrl.setQuery(query);

    NX_DEBUG(typeid(MultiserverPluginInfoHandler), "Request URL for the Server %1 (%2): %3",
        server->getName(), server->getId(), apiUrl);

    return apiUrl;
}

static std::optional<QnJsonRestResult> deserializeRemoteResult(const QByteArray& messageBody)
{
    QnJsonRestResult result;
    if (!QJson::deserialize(messageBody, &result))
        return std::nullopt;

    return result;
}

MultiserverPluginInfoHandler::MultiserverPluginInfoHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

JsonRestResponse MultiserverPluginInfoHandler::executeGet(const JsonRestRequest& request)
{
    using namespace nx::network::http;

    const QnRequestParams requestParameters = request.params;
    JsonRequestContext requestContext(request, request.owner->owner()->getPort());

    const QnResourcePool* const resourcePool = request.owner->resourcePool();

    QnMediaServerResourceList serversToCollectDataFrom =
        resourcePool->getAllServers(Qn::ResourceStatus::Online);

    if (QnLexical::deserialized<bool>(request.params[kIsLocalParameterName]))
        serversToCollectDataFrom = {request.owner->commonModule()->currentServer()};

    ExtendedPluginInfoByServer pluginInfoByServer;
    for (const QnMediaServerResourcePtr& server: serversToCollectDataFrom)
    {
        const QnUuid serverId = server->getId();
        PluginInfoExList* serverExtendedPluginInfoList = &pluginInfoByServer[serverId];
        if (serverId == moduleGUID())
            loadLocalData(serverExtendedPluginInfoList);
        else
            loadRemoteDataAsync(serverExtendedPluginInfoList, &requestContext, server);
    }

    requestContext.waitForDone();

    QnJsonContext context;
    context.setSerializeMapToObject(true);

    QJsonValue jsonResult;
    QJson::serialize(&context, pluginInfoByServer, &jsonResult);

    return jsonResult.toObject();
}

void MultiserverPluginInfoHandler::loadLocalData(
    PluginInfoExList* outExtendedPluginInfoList)
{
    if (!NX_ASSERT(outExtendedPluginInfoList))
        return;

    const PluginManager* pluginManager = serverModule()->pluginManager();
    if (!NX_ASSERT(pluginManager, "Unable to access PluginManager"))
        return;

    *outExtendedPluginInfoList = pluginManager->extendedPluginInfoList();
}

void MultiserverPluginInfoHandler::loadRemoteDataAsync(
    PluginInfoExList* outExtendedPluginInfoList,
    JsonRequestContext* inOutRequestContext,
    const QnMediaServerResourcePtr& server)
{
    const auto requestCompletionCallback =
        [outExtendedPluginInfoList, inOutRequestContext, server, this](
            SystemError::ErrorCode osErrorCode,
            int httpStatusCode,
            nx::network::http::BufferType messageBody,
            nx::network::http::HttpHeaders /*httpResponseHeaders*/)
        {
            onRemoteRequestCompletion(
                outExtendedPluginInfoList,
                inOutRequestContext,
                server,
                {osErrorCode, httpStatusCode, messageBody});
        };

    NX_DEBUG(this, "Sending a remote request to the Server %1 (%2)",
        server->getName(), server->getId());

    runMultiserverDownloadRequest(
        serverModule()->commonModule()->router(),
        buildRemoteRequestUrl(server),
        server,
        requestCompletionCallback,
        inOutRequestContext);

    return;
}

void MultiserverPluginInfoHandler::onRemoteRequestCompletion(
    nx::vms::api::PluginInfoExList* outExtendedPluginInfoList,
    JsonRequestContext* inOutRequestContext,
    const QnMediaServerResourcePtr& server,
    const HttpRequestContext& httpRequestContext)
{
    nx::utils::ScopeGuard guard(
        [inOutRequestContext]() { inOutRequestContext->requestProcessed(); });

    const bool requestSucceeded = httpRequestContext.osErrorCode == SystemError::noError
        && httpRequestContext.httpStatusCode == nx::network::http::StatusCode::ok;

    if (!requestSucceeded)
    {
        NX_DEBUG(this,
            "Request to remote server %1 (%2, %3) has failed. "
            "OS error code: %4, HTTP status code: %5",
            server->getName(), server->getUrl(), server->getId(),
            httpRequestContext.osErrorCode, httpRequestContext.httpStatusCode);

        return;
    }

    NX_DEBUG(this, "Remote request to the Server %1 (%2) has succeeded",
        server->getName(), server->getId());

    const std::optional<QnJsonRestResult> jsonResult = deserializeRemoteResult(
        httpRequestContext.messageBody);

    if (!jsonResult)
    {
        NX_DEBUG(this,
            "Unable to deserialize the response of %1 (%2, %3) to QnJsonRestResult, "
            "response content: %4",
            server->getName(), server->getId(), server->getUrl(), httpRequestContext.messageBody);

        return;
    }

    if (jsonResult->error != QnRestResult::NoError)
    {
        NX_DEBUG(this,
            "Remote server %1 (%2, %3) returned an error %4 (%5)",
            server->getName(), server->getId(), server->getUrl(),
            jsonResult->error, jsonResult->errorString);

        return;
    }

    ExtendedPluginInfoByServer remoteData;
    if (!QJson::deserialize(jsonResult->reply, &remoteData))
    {
        NX_DEBUG(this, "Unable to deserialize the response from the Server %1 (%2, %3), %4",
            server->getName(), server->getId(), server->getUrl(),
            QJson::serialize(jsonResult->reply));

        return;
    }

    inOutRequestContext->executeGuarded(
        [inOutRequestContext, &remoteData, &server, outExtendedPluginInfoList]()
        {
            if (const auto it = remoteData.find(server->getId()); it != remoteData.cend())
            {
                outExtendedPluginInfoList->insert(
                    outExtendedPluginInfoList->begin(),
                    std::make_move_iterator(it->second.begin()),
                    std::make_move_iterator(it->second.end()));
            }
        });
}

} // namespace nx::vms::server::rest
