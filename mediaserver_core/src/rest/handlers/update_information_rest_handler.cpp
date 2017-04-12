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
#include <network/module_finder.h>
#include <api/global_settings.h>

namespace {

template<typename ContextType>
QUrl getServerApiUrl(
    const QString& path, const QnMediaServerResourcePtr& server, ContextType context)
{
    QUrl result(server->getApiUrl());
    result.setPath(path);

    auto modifiedRequest = context->request();
    modifiedRequest.makeLocal(Qn::JsonFormat);
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

template<typename ReplyType, typename MergeFunction>
void requestRemotePeers(
    QnCommonModule* commonModule,
    const QString& path,
    ReplyType& outputReply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context,
    const MergeFunction& mergeFunction)
{
    const auto systemName = commonModule->globalSettings()->systemName();

    auto servers = QSet<QnMediaServerResourcePtr>::fromList(commonModule->resourcePool()->getAllServers(Qn::Online));

    for (const auto& moduleInformation: commonModule->moduleFinder()->foundModules())
    {
        if (moduleInformation.systemName != systemName)
            continue;

        const auto server =
            commonModule->resourcePool()->getResourceById<QnMediaServerResource>(moduleInformation.id);
        if (!server)
            continue;

        servers.insert(server);
    }

    for (const auto& server: servers)
    {
        const auto completionFunc =
            [&outputReply, context, serverId = server->getId(), &mergeFunction](
                SystemError::ErrorCode /*osErrorCode*/,
                int statusCode,
                nx_http::BufferType body)
            {
                ReplyType reply;
                bool success = false;

                const auto httpCode = static_cast<nx_http::StatusCode::Value>(statusCode);
                if (httpCode == nx_http::StatusCode::ok)
                    reply = QJson::deserialized(body, reply, &success);

                const auto updateOutputDataCallback =
                    [&reply, success, &outputReply, context, &serverId, &mergeFunction]()
                    {
                        mergeFunction(serverId, success, reply, outputReply);
                        context->requestProcessed();
                    };

                context->executeGuarded(updateOutputDataCallback);
            };

        const QUrl apiUrl = getServerApiUrl(path, server, context);
        runMultiserverDownloadRequest(commonModule->router(), apiUrl, server, completionFunc, context);
        context->waitForDone();
    }
}

void loadFreeSpaceRemotely(
    QnCommonModule* commonModule,
    const QString& path,
    QnUpdateFreeSpaceReply& outputReply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    auto mergeFunction =
        [](
            const QnUuid& serverId,
            bool success,
            const QnUpdateFreeSpaceReply& reply,
            QnUpdateFreeSpaceReply& outputReply)
        {
            if (success)
            {
                const auto freeSpace = reply.freeSpaceByServerId.value(serverId, -1);
                if (freeSpace > 0)
                    outputReply.freeSpaceByServerId[serverId] = freeSpace;
            }
        };

    requestRemotePeers(commonModule, path, outputReply, context, mergeFunction);
}

void checkCloudHostRemotely(
    QnCommonModule* commonModule,
    const QString& path,
    QnCloudHostCheckReply& outputReply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    auto mergeFunction =
        [](
            const QnUuid& serverId,
            bool success,
            const QnCloudHostCheckReply& reply,
            QnCloudHostCheckReply& outputReply)
        {
            if (success && !reply.cloudHost.isEmpty() && reply.cloudHost != outputReply.cloudHost)
                outputReply.failedServers.append(serverId);
        };

    requestRemotePeers(commonModule, path, outputReply, context, mergeFunction);
}

} // namespace

int QnUpdateInformationRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(processor->resourcePool(), params);

    QnMultiserverRequestContext<QnEmptyRequestData> context(
        request, processor->owner()->getPort());

    if (path.endsWith(lit("/freeSpaceForUpdateFiles")))
    {
        QnUpdateFreeSpaceReply reply;
        const auto moduleGuid = processor->commonModule()->moduleGUID();
        reply.freeSpaceByServerId[moduleGuid] = freeSpaceForUpdate();

        if (!request.isLocal)
            loadFreeSpaceRemotely(processor->commonModule(), path, reply, &context);

        QnFusionRestHandlerDetail::serialize(reply, result, contentType, request.format);
        return nx_http::StatusCode::ok;
    }
    else if (path.endsWith(lit("/checkCloudHost")))
    {
        QnCloudHostCheckReply reply;
        reply.cloudHost = processor->globalSettings()->cloudHost();

        if (!request.isLocal)
            checkCloudHostRemotely(processor->commonModule(), path, reply, &context);

        QnFusionRestHandlerDetail::serialize(reply, result, contentType, request.format);
        return nx_http::StatusCode::ok;
    }

    return QnFusionRestHandler::makeError(nx_http::StatusCode::badRequest,
        lit("Unknown operation"),
        &result, &contentType, request.format, request.extraFormatting,
        QnRestResult::CantProcessRequest);
}
