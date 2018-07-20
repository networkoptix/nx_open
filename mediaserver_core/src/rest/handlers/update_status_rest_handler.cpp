#include "update_status_rest_handler.h"
#include "private/multiserver_request_helper.h"
#include <api/helpers/empty_request_data.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/update/common_update_manager.h>
#include <media_server/media_server_module.h>

namespace {

void checkUpdateStatusRemotely(
    QnCommonModule* commonModule,
    const QString& path,
    QList<nx::UpdateStatus>* reply,
    QnMultiserverRequestContext<QnEmptyRequestData>* context)
{
    static const QString kOfflineMessage = "peer is offline";
    auto mergeFunction =
        [](
            const QnUuid& serverId,
            bool success,
            QList<nx::UpdateStatus>& reply,
            QList<nx::UpdateStatus>& outputReply)
        {
            if (success)
            {
                NX_ASSERT(reply.size() == 1);
                outputReply.append(reply[0]);
            }
            else
            {
                outputReply.append(
                    nx::UpdateStatus(serverId, nx::UpdateStatus::Code::offline, kOfflineMessage));
            }
        };

    detail::requestRemotePeers(commonModule, path, *reply, context, mergeFunction);
    auto offlineServers = QSet<QnMediaServerResourcePtr>::fromList(
        commonModule->resourcePool()->getAllServers(Qn::Offline));
    for (const auto offlineServer : offlineServers)
    {
        if (std::find_if(
                reply->cbegin(),
                reply->cend(),
                [&offlineServer](const nx::UpdateStatus& updateStatus)
                {
                    return updateStatus.serverId == offlineServer->getId();
                }) != reply->cend())
        {
            continue;
        }
        reply->append(
            nx::UpdateStatus(
                offlineServer->getId(),
                nx::UpdateStatus::Code::offline,
                kOfflineMessage));
    }
}

} // namespace

int QnUpdateStatusRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor*processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(),
        params);

    QnMultiserverRequestContext<QnEmptyRequestData> context(
        request,
        processor->owner()->getPort());

    QList<nx::UpdateStatus> reply;
    reply.append(qnServerModule->updateManager()->status());

    if (!request.isLocal)
        checkUpdateStatusRemotely(processor->commonModule(), path, &reply, &context);

    QnFusionRestHandlerDetail::serialize(reply, result, contentType, request.format);
    return nx::network::http::StatusCode::ok;
}
