#include "update_information_rest_handler.h"

#include <QtCore/QDir>

#include <utils/common/util.h>
#include <media_server/settings.h>
#include <api/model/update_information_reply.h>
#include <api/helpers/empty_request_data.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_listener.h>

namespace {

template<typename ContextType>
QUrl getServerApiUrl(
    const QString& path, const QnMediaServerResourcePtr& server, ContextType context)
{
    QUrl result(server->getApiUrl());
    result.setPath(path);

    auto modifiedRequest = context->request();
    modifiedRequest.makeLocal();
    result.setQuery(modifiedRequest.toUrlQuery());

    return result;
}

qint64 freeSpaceForUpdate()
{
    auto updatesDir = MSSettings::roSettings()->value("dataDir").toString();
    if (updatesDir.isEmpty())
        updatesDir = QDir::tempPath();

    return getDiskFreeSpace(updatesDir);
}

void loadFreeSpaceRemotely(
    const QString& path,
    QnUpdateFreeSpaceReply& outputReply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    const auto moduleGuid = qnCommon->moduleGUID();

    for (const auto server: qnResPool->getAllServers(Qn::Online))
    {
        const auto serverId = server->getId();
        if (serverId == moduleGuid)
            continue;

        const auto completionFunc = [&outputReply, context, &serverId]
            (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType body)
            {
                Q_UNUSED(osErrorCode)

                qint64 freeSpace = -1;

                const auto httpCode = static_cast<nx_http::StatusCode::Value>(statusCode);
                if (httpCode == nx_http::StatusCode::ok)
                {
                    QnUpdateFreeSpaceReply reply;
                    bool success = false;
                    reply = QJson::deserialized(body, reply, &success);
                    if (success)
                        freeSpace = reply.freeSpaceByServerId.value(serverId, -1);
                }

                const auto updateOutputDataCallback =
                    [freeSpace, &outputReply, context, httpCode, &serverId]()
                    {
                        outputReply.freeSpaceByServerId[serverId] = freeSpace;
                        context->requestProcessed();
                    };

                context->executeGuarded(updateOutputDataCallback);
            };

        const QUrl apiUrl = getServerApiUrl(path, server, context);
        runMultiserverDownloadRequest(apiUrl, server, completionFunc, context);
        context->waitForDone();
    }
}

} // namespace

int QnUpdateInformationRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    Q_UNUSED(path)

    if (path.endsWith(lit("/freeSpaceForUpdateFiles")))
    {
        QnUpdateFreeSpaceReply freeSpaceResult;

        auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(params);
        QnMultiserverRequestContext<QnEmptyRequestData> context(
            request, processor->owner()->getPort());

        const auto moduleGuid = qnCommon->moduleGUID();

        freeSpaceResult.freeSpaceByServerId[moduleGuid] = freeSpaceForUpdate();

        if (!request.isLocal)
            loadFreeSpaceRemotely(path, freeSpaceResult, &context);

        QnFusionRestHandlerDetail::serialize(freeSpaceResult, result, contentType, request.format);
    }

    return nx_http::StatusCode::ok;
}
