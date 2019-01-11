#include "update_status_rest_handler.h"
#include "private/multiserver_request_helper.h"
#include <rest/server/rest_connection_processor.h>


QnUpdateStatusRestHandler::QnUpdateStatusRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnUpdateStatusRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(), params);

    QnMultiserverRequestContext<QnEmptyRequestData> context(
        request, processor->owner()->getPort());

    QList<nx::update::Status> reply;
    if (request.isLocal)
    {
        reply.append(serverModule()->updateManager()->status());
    }
    else
    {
        detail::checkUpdateStatusRemotely(serverModule()->updateManager()->participants(),
            processor->commonModule(), path, &reply, &context);
    }

    QnFusionRestHandlerDetail::serialize(reply, result, contentType, request.format);
    return nx::network::http::StatusCode::ok;
}
