#include "time_sync_rest_handler.h"

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>

#include <rest/server/rest_connection_processor.h>

#include <local_connection_factory.h>

namespace nx {
namespace mediaserver {

QnTimeSyncRestHandler::QnTimeSyncRestHandler()
{
}

int QnTimeSyncRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    QByteArray& result,
    QByteArray& /*contentType*/,
    const QnRestConnectionProcessor* connection)
{
    auto ecConnection = connection->commonModule()->ec2Connection();
    if (!ecConnection)
        return nx::network::http::StatusCode::internalServerError;
    result = QByteArray::number(ecConnection->timeSyncManager()->getSyncTime().count());
    return nx::network::http::StatusCode::ok;
}

int QnTimeSyncRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParamList& /*params*/,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& /*result*/,
    QByteArray& /*resultContentType*/,
    const QnRestConnectionProcessor* /*owner*/)
{
    return nx::network::http::StatusCode::badRequest;
}

} // namespace mediaserver
} // namespace nx
