#include "multiserver_get_hardware_ids_rest_handler.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <api/model/hardware_ids_reply.h>
#include <network/tcp_listener.h>

#include <nx/kit/debug.h>

namespace {

using DummyType = int;
typedef QnMultiserverRequestContext<DummyType> QnGetHardwareIdsRequestContext;

static void loadRemoteDataAsync(
    QnCommonModule* commonModule,
    ApiServerHardwareIdsDataList& outputData,
    const QnMediaServerResourcePtr& server,
    QnGetHardwareIdsRequestContext* context,
    const QString& urlPath)
{
    auto requestCompletionFunc =
        [context, &outputData, server]
        (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody)
        {
            bool success = false;
            ApiServerHardwareIdsData remoteData;
            QStringList serverData;
            if (osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok)
            {
                QnJsonRestResult jsonReply;
                success = QJson::deserialize(msgBody, &jsonReply)
                    && QJson::deserialize(jsonReply.reply, &serverData);
            }
            if (success)
            {
                remoteData.serverId = server->getId();
                remoteData.hardwareIds = serverData;
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

QnMultiserverGetHardwareIdsRestHandler::QnMultiserverGetHardwareIdsRestHandler(
    const QString& getHardwareIdsApiMethodPath)
    :
    QnFusionRestHandler(),
    m_getHardwareIdsApiMethodPath(getHardwareIdsApiMethodPath)
{
}

int QnMultiserverGetHardwareIdsRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    ApiServerHardwareIdsDataList outputData;
    const auto ownerPort = owner->owner()->getPort();
    QnGetHardwareIdsRequestContext context(DummyType(), ownerPort);

    auto onlineServers = owner->commonModule()->resourcePool()->getAllServers(Qn::Online);
    for (const auto& server: onlineServers)
    {
        loadRemoteDataAsync(
            owner->commonModule(), outputData, server, &context, m_getHardwareIdsApiMethodPath);
    }

    context.waitForDone();
    QnRestResult restResult;
    QnFusionRestHandlerDetail::serializeRestReply(
        outputData, params, result, contentType, restResult);

    return nx_http::StatusCode::ok;
}
