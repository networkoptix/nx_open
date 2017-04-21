#include "distributed_file_downloader_rest_handler.h"

#include <nx/network/http/httptypes.h>
#include <nx/vms/common/distributed_file_downloader.h>

#include <media_server/media_server_module.h>

int QnDistributedFileDownloaderRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    return executePost(
        path,
        params,
        QByteArray(),
        QByteArray(),
        result,
        contentType,
        processor);
}

int QnDistributedFileDownloaderRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& bodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    using nx::vms::common::DistributedFileDownloader;

    auto downloader = qnServerModule->findInstance<DistributedFileDownloader>();
    if (!downloader)
        return nx_http::StatusCode::internalServerError;

    return nx_http::StatusCode::ok;
}
