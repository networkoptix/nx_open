#include "start_update_rest_handler.h"
#include <media_server/media_server_module.h>
#include <api/global_settings.h>
#include <nx/mediaserver/settings.h>

int QnStartUpdateRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& srcBodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    qnServerModule->commonModule()->globalSettings()->setUpdateInformation(body);
    qnServerModule->commonModule()->globalSettings()->synchronizeNow();

    return nx::network::http::StatusCode::ok;
}
