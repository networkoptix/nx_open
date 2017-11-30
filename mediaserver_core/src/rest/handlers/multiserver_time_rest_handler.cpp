#include "multiserver_time_rest_handler.h"

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

namespace {

using DummyType = int;
typedef QnMultiserverRequestContext<DummyType> QnTimeRequestContext;

static void loadRemoteDataAsync(
    QnCommonModule* commonModule,
    ApiMultiserverServerDateTimeDataList& outputData,
    const QnMediaServerResourcePtr& server,
    QnTimeRequestContext* context,
    const QString& urlPath)
{
    auto requestCompletionFunc =
        [context, &outputData, server]
        (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody)
        {
            bool success = false;
            ApiServerDateTimeData remoteData;
            QnTimeReply timeData;
            if (osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok)
            {
                QnJsonRestResult jsonReply;
                success = QJson::deserialize(msgBody, &jsonReply)
                    && QJson::deserialize(jsonReply.reply, &timeData);
            }
            if (success)
            {
                remoteData.serverId = server->getId();
                remoteData.timeSinseEpochMs = timeData.utcTime;
                remoteData.timeZoneOffsetMs = timeData.timeZoneOffset;
                remoteData.timeZoneId = timeData.timezoneId;
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
    apiUrl.setQuery("local"); //< Use device (OS) time.

    auto router = commonModule->router();
    runMultiserverDownloadRequest(router, apiUrl, server, requestCompletionFunc, context);
}

} // namespace

QnMultiserverTimeRestHandler::QnMultiserverTimeRestHandler(const QString& getTimeApiMethodPath):
    QnFusionRestHandler(),
    m_getTimeApiMethodPath(getTimeApiMethodPath)
{
}

int QnMultiserverTimeRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    ApiMultiserverServerDateTimeDataList outputData;
    const auto ownerPort = owner->owner()->getPort();
    QnTimeRequestContext context(DummyType(), ownerPort);

    auto onlineServers = owner->commonModule()->resourcePool()->getAllServers(Qn::Online);
    for (const auto& server: onlineServers)
    {
        loadRemoteDataAsync(
            owner->commonModule(), outputData, server, &context, m_getTimeApiMethodPath);
    }

    context.waitForDone();
    QnRestResult restResult;
    QnFusionRestHandlerDetail::serializeRestReply(
        outputData, params, result, contentType, restResult);

    return nx_http::StatusCode::ok;
}
