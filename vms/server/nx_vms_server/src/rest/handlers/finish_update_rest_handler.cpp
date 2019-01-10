#include "finish_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <nx/update/common_update_manager.h>
#include "private/multiserver_request_helper.h"
#include <rest/server/rest_connection_processor.h>

QnFinishUpdateRestHandler::QnFinishUpdateRestHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int QnFinishUpdateRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<QnEmptyRequestData>(
        processor->resourcePool(),
        params);
    return QnFusionRestHandler::makeError(
        nx::network::http::StatusCode::ok,
        "Missing required parameter 'peers'",
        &result,
        &resultContentType,
        Qn::JsonFormat,
        request.extraFormatting,
        QnRestResult::MissingParameter);
    return nx::network::http::StatusCode::ok;
}
