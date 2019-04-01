#include "server_time_handler.h"

#include <nx/vms/api/data/time_reply.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <api/model/time_reply.h>
#include <network/tcp_listener.h>
#include <nx/utils/elapsed_timer.h>

using namespace nx::vms::api;

namespace {

typedef QnMultiserverRequestContext<nx::utils::ElapsedTimer> ServerTimeRequestContext;

static void loadRemoteDataAsync(
    QnCommonModule* commonModule,
    ServerTimeReplyList& outputData,
    const QnMediaServerResourcePtr& server,
    ServerTimeRequestContext* context,
    const QString& urlPath)
{
    auto requestCompletionFunc =
        [context, &outputData, server](SystemError::ErrorCode osErrorCode, int statusCode,
            nx::network::http::BufferType msgBody, nx::network::http::HttpHeaders /*httpHeaders*/)
        {
            bool success = false;
            TimeReply timeData;
            ServerTimeReply remoteData;
            if (osErrorCode == SystemError::noError && statusCode == nx::network::http::StatusCode::ok)
            {
                QnJsonRestResult jsonReply;
                success = QJson::deserialize(msgBody, &jsonReply)
                    && QJson::deserialize(jsonReply.reply, &timeData);
            }
            if (success)
            {
                remoteData.serverId = server->getId();
                remoteData.osTime = timeData.osTime - context->request().elapsed() / 2;
                remoteData.vmsTime = timeData.vmsTime - context->request().elapsed() / 2;
                remoteData.timeZoneOffset = timeData.timeZoneOffset;
            }
            context->executeGuarded(
                [context, success, &remoteData, &outputData]()
                {
                    if (success)
                        outputData.push_back(std::move(remoteData));
                    context->requestProcessed();
                });
        };

    nx::utils::Url apiUrl(server->getApiUrl());
    apiUrl.setPath(urlPath);

    auto router = commonModule->router();
    runMultiserverDownloadRequest(router, apiUrl, server, requestCompletionFunc, context);
}

} // namespace

namespace nx::vms::server::rest {

 ServerTimeHandler::ServerTimeHandler(const QString &getTimeHandlerPath):
     m_getTimeHandlerPath(getTimeHandlerPath)
 {
 }

int ServerTimeHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    ServerTimeReplyList outputData;
    const auto ownerPort = owner->owner()->getPort();
    nx::utils::ElapsedTimer timer;
    timer.restart();
    ServerTimeRequestContext context(timer, ownerPort);

    auto onlineServers = owner->commonModule()->resourcePool()->getAllServers(Qn::Online);
    for (const auto& server: onlineServers)
    {
        loadRemoteDataAsync(
            owner->commonModule(), outputData, server, &context, m_getTimeHandlerPath);
    }

    context.waitForDone();

    for (auto &record : outputData)
    {
        record.osTime += timer.elapsed() / 2;
        record.vmsTime += timer.elapsed() / 2;
    }

    result.setReply(outputData);
    return nx::network::http::StatusCode::ok;
}

} // namespace nx::vms::server::rest