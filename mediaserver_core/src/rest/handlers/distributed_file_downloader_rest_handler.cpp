#include "distributed_file_downloader_rest_handler.h"

#include <nx/network/http/httptypes.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/common/distributed_file_downloader.h>
#include <rest/server/json_rest_result.h>
#include <media_server/media_server_module.h>

using nx::vms::common::distributed_file_downloader::Downloader;
using nx::vms::common::distributed_file_downloader::FileInformation;

namespace {

static const QByteArray kJsonContentType("application/json");
static const QByteArray kOctetStreamContentType("application/octet-stream");

enum class Command
{
    addDownload,
    removeDownload,
    chunkChecksums,
    downloadChunk,
    uploadChunk,
    status,
    unknown
};

Command commandFromPath(const QString& path)
{
    static const QHash<QString, Command> commandByString{
        {"addDownload", Command::addDownload},
        {"removeDownload", Command::removeDownload},
        {"chunkChecksums", Command::chunkChecksums},
        {"downloadChunk", Command::downloadChunk},
        {"uploadChunk", Command::uploadChunk},
        {"status", Command::status}
    };

    // path is expected to look like "/api/downloader/status".
    const auto command = path.section('/', 3, 3);
    return commandByString.value(command, Command::unknown);
}

class RequestHelper
{
public:
    RequestHelper(
        QnDistributedFileDownloaderRestHandler* handler,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& resultContentType);

    int handleAddDownload();
    int handleRemoveDownload();
    int handleGetChunkChecksums();
    int handleDownloadChunk();
    int handleUploadChunk(const QByteArray& body, const QByteArray& contentType);
    int handleStatus();

    int makeError(
        int httpStatusCode,
        const QnRestResult::Error& error,
        const QString& errorString);
    int makeInvalidParameterError(
        const QString& parameter,
        const QnRestResult::Error& error = QnRestResult::InvalidParameter);
    int makeDownloaderError(Downloader::ErrorCode errorCode);

    QnDistributedFileDownloaderRestHandler* handler;
    Downloader* const downloader;
    const QnRequestParamList& params;
    QByteArray& result;
    QByteArray& resultContentType;
};

RequestHelper::RequestHelper(
    QnDistributedFileDownloaderRestHandler* handler,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType)
    :
    handler(handler),
    downloader(qnServerModule->findInstance<Downloader>()),
    params(params),
    result(result),
    resultContentType(resultContentType)
{
}

int RequestHelper::handleAddDownload()
{
    FileInformation fileInfo;

    fileInfo.name = params.value("fileName");
    if (fileInfo.name.isEmpty())
        return makeInvalidParameterError("fileName", QnRestResult::MissingParameter);

    const auto sizeString = params.value("size");
    if (!sizeString.isEmpty())
    {
        bool ok;
        fileInfo.size = sizeString.toInt(&ok);
        if (!ok || fileInfo.size <= 0)
            return makeInvalidParameterError("size");
    }

    const auto md5String = params.value("md5").toUtf8();
    if (!md5String.isEmpty())
    {
        fileInfo.md5 = QByteArray::fromBase64(md5String);
        /* QByteArray::fromBase64() silently ignores all invalid characters,
           so converting md5 back to base64 to check the checksum format validity. */
        if (fileInfo.md5.toBase64() != md5String)
            return makeInvalidParameterError("md5");
    }

    const auto urlString = params.value("url");
    if (!urlString.isEmpty())
    {
        fileInfo.url = QUrl(urlString);
        if (!fileInfo.url.isValid())
            return makeInvalidParameterError("url");
    }

    const auto errorCode = downloader->addFile(fileInfo);
    if (errorCode != Downloader::ErrorCode::noError)
        return makeDownloaderError(errorCode);

    return nx_http::StatusCode::ok;
}

int RequestHelper::handleRemoveDownload()
{
    const auto fileName = params.value("fileName");
    if (fileName.isEmpty())
        return makeInvalidParameterError("fileName", QnRestResult::MissingParameter);

    const bool deleteData = params.value("deleteData", "true") != "false";

    const auto errorCode = downloader->deleteFile(fileName, deleteData);
    if (errorCode != Downloader::ErrorCode::noError)
        return makeDownloaderError(errorCode);

    return nx_http::StatusCode::ok;
}

int RequestHelper::handleGetChunkChecksums()
{
    const auto fileName = params.value("fileName");
    if (fileName.isEmpty())
        return makeInvalidParameterError("fileName", QnRestResult::MissingParameter);

    const auto& fileInfo = downloader->fileInformation(fileName);
    if (!fileInfo.isValid())
        return makeInvalidParameterError("fileName: File does not exist.");

    const auto& checksums = downloader->getChunkChecksums(fileInfo.name);

    QnFusionRestHandlerDetail::serializeJsonRestReply(
        checksums, params, result, resultContentType, QnRestResult());
    return nx_http::StatusCode::ok;
}

int RequestHelper::handleDownloadChunk()
{
    const auto fileName = params.value("fileName");
    if (fileName.isEmpty())
        return makeInvalidParameterError("fileName", QnRestResult::MissingParameter);

    bool ok;
    const int chunkIndex = params.value("index").toInt(&ok);
    if (!ok)
        return makeInvalidParameterError("index", QnRestResult::MissingParameter);

    QByteArray data;
    const auto errorCode = downloader->readFileChunk(fileName, chunkIndex, data);
    if (errorCode != Downloader::ErrorCode::noError)
        return makeDownloaderError(errorCode);

    result = data;
    resultContentType = kOctetStreamContentType;
    return nx_http::StatusCode::ok;
}

int RequestHelper::handleUploadChunk(const QByteArray& body, const QByteArray& contentType)
{
    const auto fileName = params.value("fileName");
    if (fileName.isEmpty())
        return makeInvalidParameterError("fileName", QnRestResult::MissingParameter);

    bool ok;
    const int chunkIndex = params.value("index").toInt(&ok);
    if (!ok)
        return makeInvalidParameterError("index", QnRestResult::MissingParameter);

    if (contentType != kOctetStreamContentType)
    {
        return makeError(
            nx_http::StatusCode::badRequest,
            QnRestResult::CantProcessRequest,
            lit("Only %1 Content-Type is supported.").arg(
                QLatin1String(kOctetStreamContentType)));
    }

    const auto errorCode = downloader->writeFileChunk(fileName, chunkIndex, body);
    if (errorCode != Downloader::ErrorCode::noError)
        return makeDownloaderError(errorCode);

    return nx_http::StatusCode::ok;
}

int RequestHelper::handleStatus()
{
    const auto fileName = params.value("fileName");
    if (!fileName.isEmpty())
    {
        const auto& fileInfo = downloader->fileInformation(fileName);
        if (!fileInfo.isValid())
            return makeInvalidParameterError("fileName: File does not exist.");

        QnFusionRestHandlerDetail::serializeJsonRestReply(
            fileInfo, params, result, resultContentType, QnRestResult());
    }
    else
    {
        QList<FileInformation> infoList;
        for (const auto& fileName: downloader->files())
        {
            const auto& fileInfo = downloader->fileInformation(fileName);
            infoList.append(fileInfo);
        }

        QnFusionRestHandlerDetail::serializeJsonRestReply(
            infoList, params, result, resultContentType, QnRestResult());
    }

    return nx_http::StatusCode::ok;
}

int RequestHelper::makeError(
    int httpStatusCode,
    const QnRestResult::Error& error,
    const QString& errorString)
{
    return handler->makeError(
        httpStatusCode,
        errorString,
        &result,
        &resultContentType,
        Qn::JsonFormat,
        false,
        error);
}

int RequestHelper::makeInvalidParameterError(
    const QString& parameter, const QnRestResult::Error& error)
{
    return makeError(
        nx_http::StatusCode::invalidParameter,
        QnRestResult::InvalidParameter,
        parameter);
}

int RequestHelper::makeDownloaderError(Downloader::ErrorCode errorCode)
{
    return makeError(
        nx_http::StatusCode::internalServerError,
        QnRestResult::CantProcessRequest,
        lit("DistributedFileDownloader returned error: %1").arg(
            QnLexical::serialized(errorCode)));
}

} // namespace

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
    const QnRestConnectionProcessor* /*processor*/)
{
    RequestHelper helper(this, params, result, resultContentType);

    if (!helper.downloader)
    {
        return helper.makeError(
            nx_http::StatusCode::internalServerError,
            QnRestResult::CantProcessRequest,
            "DistributedFileDownloader is not initialized.");
    }

    const auto command = commandFromPath(path);
    switch (command)
    {
        case Command::addDownload:
            return helper.handleAddDownload();

        case Command::removeDownload:
            return helper.handleRemoveDownload();

        case Command::chunkChecksums:
            return helper.handleGetChunkChecksums();

        case Command::downloadChunk:
            return helper.handleDownloadChunk();

        case Command::uploadChunk:
            return helper.handleUploadChunk(body, bodyContentType);

        case Command::status:
            return helper.handleStatus();

        default:
            return helper.makeError(
                nx_http::StatusCode::badRequest,
                QnRestResult::InvalidParameter,
                "Invalid path specified.");
    }

    return nx_http::StatusCode::ok;
}
