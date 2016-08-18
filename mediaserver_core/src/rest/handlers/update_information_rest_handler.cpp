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

nx_http::StatusCode::Value loadFreeSpaceRemotely(
    const QString& path,
    QnMultiServerUpdateFreeSpaceReply& outputReply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    const auto moduleGuid = qnCommon->moduleGUID();

    for (const auto server: qnResPool->getAllServers(Qn::Online))
    {
        if (server->getId() == moduleGuid)
            continue;

        auto resultCode = nx_http::StatusCode::notFound;

        const auto completionFunc = [&outputReply, &resultCode, context]
            (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType body)
            {
                Q_UNUSED(osErrorCode)

                const auto httpCode = static_cast<nx_http::StatusCode::Value>(statusCode);
                if (httpCode != nx_http::StatusCode::ok)
                {
                    resultCode = httpCode;
                    context->requestProcessed();
                    return;
                }

                QnMultiServerUpdateFreeSpaceReply reply;
                bool success = false;
                reply = QJson::deserialized(body, reply, &success);
                if (!success)
                {
                    resultCode = nx_http::StatusCode::notFound;
                    context->requestProcessed();
                    return;
                }

                const auto updateOutputDataCallback =
                    [&reply, &outputReply, &resultCode, context, httpCode]()
                    {
                        outputReply.append(reply);
                        resultCode = httpCode;
                        context->requestProcessed();
                    };

                context->executeGuarded(updateOutputDataCallback);
            };

        const QUrl apiUrl = getServerApiUrl(path, server, context);
        runMultiserverDownloadRequest(apiUrl, server, completionFunc, context);
        context->waitForDone();

        if (resultCode != nx_http::StatusCode::ok)
            return resultCode;
    }

    return nx_http::StatusCode::ok;
}

} // namespace

int QnUpdateInformationRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* processor)
{
    Q_UNUSED(path)

    if (path.endsWith(lit("/freeSpaceForUpdateFiles")))
    {
        QnMultiServerUpdateFreeSpaceReply freeSpaceResult;

        auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(params);
        QnMultiserverRequestContext<QnEmptyRequestData> context(
            request, processor->owner()->getPort());

        const auto moduleGuid = qnCommon->moduleGUID();

        freeSpaceResult.append(QnUpdateFreeSpaceReply(moduleGuid, freeSpaceForUpdate()));
        auto resultCode = nx_http::StatusCode::ok;

        if (!request.isLocal)
            resultCode = loadFreeSpaceRemotely(path, freeSpaceResult, &context);

        if (resultCode == nx_http::StatusCode::ok)
            result.setReply(freeSpaceResult);
        else
            result.setError(QnRestResult::CantProcessRequest);

        return resultCode;
    }

    return nx_http::StatusCode::ok;
}
