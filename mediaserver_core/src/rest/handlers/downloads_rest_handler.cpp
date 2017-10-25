#include "downloads_rest_handler.h"

#include <nx/network/http/http_types.h>
#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <rest/server/json_rest_result.h>
#include <rest/helpers/request_helpers.h>
#include <media_server/media_server_module.h>

using nx::vms::common::p2p::downloader::Downloader;
using nx::vms::common::p2p::downloader::FileInformation;
using nx::vms::common::p2p::downloader::ResultCode;

namespace {

static const QByteArray kJsonContentType("application/json");
static const QByteArray kOctetStreamContentType("application/octet-stream");
static const int kDownloadRequestTimeoutMs = 10 * 60 * 1000;
static const int kMaxChunkSize = 10 * 1024 * 1024;

struct Request
{
    enum class Subject
    {
        unknown,
        file,
        chunk,
        status,
        checksums
    };

    QString fileName;
    int chunkIndex = -1;
    Subject subject = Subject::unknown;

    Request(const QString& path)
    {
        auto sections = path.splitRef('/', QString::SkipEmptyParts);
        if (sections.size() < 2)
            return;

        // Strip "/api/downloads".
        sections.remove(0, 2);

        if (sections.last() == "status")
        {
            subject = Subject::status;
            sections.removeLast();
        }
        else if (sections.last() == "checksums")
        {
            subject = Subject::checksums;
            sections.removeLast();
        }
        else if (sections.last() == "chunks")
        {
            return;
        }
        else if (sections.size() > 2 && sections[sections.size() - 2] == "chunks")
        {
            bool ok = false;
            chunkIndex = sections.last().toInt(&ok);
            if (!ok)
            {
                chunkIndex = -1;
                return;
            }

            subject = Subject::chunk;
            sections.remove(sections.size() - 2, 2);
        }

        if (!sections.isEmpty())
        {
            const auto start = sections.first().position();
            const auto end = sections.last().position() + sections.last().length();
            fileName = path.mid(start, end - start);
        }
        if (!fileName.isEmpty())
        {
            if (subject == Subject::unknown)
                subject = Subject::file;
        }
    }

    bool isValid() const
    {
        return subject != Subject::unknown;
    }
};

class Helper
{
public:
    Helper(
        QnDownloadsRestHandler* handler,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& resultContentType);

    int handleAddDownload(const QString& fileName);
    int handleRemoveDownload(const QString& fileName);
    int handleGetChunkChecksums(const QString& fileName);
    int handleDownloadChunk(const QString& fileName, int chunkIndex);
    int handleDownloadChunkFromInternet(const QString& fileName, int chunkIndex);
    int handleUploadChunk(
        const QString& fileName,
        int chunkIndex,
        const QByteArray& body,
        const QByteArray& contentType);
    int handleStatus(const QString& fileName);

    int makeError(
        int httpStatusCode,
        const QnRestResult::Error& error,
        const QString& errorString);
    int makeInvalidParameterError(
        const QString& parameter,
        const QnRestResult::Error& error = QnRestResult::InvalidParameter);
    int makeFileError(const QString& fileName);
    int makeDownloaderError(ResultCode errorCode);

    bool hasDownloader() const;

private:
    QnDownloadsRestHandler* handler;
    Downloader* const downloader;
    const QnRequestParamList& params;
    QByteArray& result;
    QByteArray& resultContentType;
};

Helper::Helper(
    QnDownloadsRestHandler* handler,
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

int Helper::handleAddDownload(const QString& fileName)
{
    FileInformation fileInfo(fileName);

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
        fileInfo.md5 = QByteArray::fromHex(md5String);
        /* QByteArray::fromHex() silently ignores all invalid characters,
           so converting md5 back to hex to check the checksum format validity. */
        if (fileInfo.md5.toHex() != md5String)
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
    if (errorCode != ResultCode::ok)
        return makeDownloaderError(errorCode);

    return nx_http::StatusCode::ok;
}

int Helper::handleRemoveDownload(const QString& fileName)
{
    const bool deleteData = params.value("deleteData", "true") != "false";

    const auto errorCode = downloader->deleteFile(fileName, deleteData);
    if (errorCode != ResultCode::ok)
        return makeDownloaderError(errorCode);

    return nx_http::StatusCode::ok;
}

int Helper::handleGetChunkChecksums(const QString& fileName)
{
    const auto& fileInfo = downloader->fileInformation(fileName);
    if (!fileInfo.isValid())
        return makeFileError(fileName);

    const auto& checksums = downloader->getChunkChecksums(fileInfo.name);

    QnFusionRestHandlerDetail::serializeJsonRestReply(
        checksums, params, result, resultContentType, QnRestResult());
    return nx_http::StatusCode::ok;
}

int Helper::handleDownloadChunk(const QString& fileName, int chunkIndex)
{
    if (params.value("fromInternet", "false") == "true")
        return handleDownloadChunkFromInternet(fileName, chunkIndex);

    QByteArray data;
    const auto errorCode = downloader->readFileChunk(fileName, chunkIndex, data);
    if (errorCode != ResultCode::ok)
        return makeDownloaderError(errorCode);

    result = data;
    resultContentType = kOctetStreamContentType;
    return nx_http::StatusCode::ok;
}

int Helper::handleDownloadChunkFromInternet(const QString& fileName, int chunkIndex)
{
    const QUrl url = params.value("url");
    if (url.isEmpty())
        return makeInvalidParameterError("url", QnRestResult::MissingParameter);
    else if (!url.isValid())
        return makeInvalidParameterError("url", QnRestResult::InvalidParameter);

    bool ok = false;
    const qint64 chunkSize = params.value("chunkSize").toLongLong(&ok);
    if (!ok)
        return makeInvalidParameterError("chunkSize", QnRestResult::MissingParameter);
    if (chunkSize > kMaxChunkSize)
        return makeInvalidParameterError("chunkSize", QnRestResult::InvalidParameter);

    const auto fileInfo = downloader->fileInformation(fileName);
    bool useDownloader = fileInfo.isValid()
        && fileInfo.url == url
        && fileInfo.chunkSize == chunkSize
        && chunkIndex < fileInfo.downloadedChunks.size();

    if (useDownloader && fileInfo.downloadedChunks[chunkIndex])
    {
        QByteArray data;
        const auto errorCode = downloader->readFileChunk(fileName, chunkIndex, data);
        if (errorCode == ResultCode::ok)
        {
            result = data;
            resultContentType = kOctetStreamContentType;
            return nx_http::StatusCode::ok;
        }
    }

    nx_http::HttpClient httpClient;
    httpClient.setResponseReadTimeoutMs(kDownloadRequestTimeoutMs);
    httpClient.setSendTimeoutMs(kDownloadRequestTimeoutMs);
    httpClient.setMessageBodyReadTimeoutMs(kDownloadRequestTimeoutMs);

    const qint64 pos = chunkIndex * chunkSize;
    httpClient.addAdditionalHeader("Range",
        lit("bytes=%1-%2").arg(pos).arg(pos + chunkSize - 1).toLatin1());

    if (!httpClient.doGet(url) || !httpClient.response())
        return nx_http::StatusCode::internalServerError;

    const auto status = httpClient.response()->statusLine.statusCode;

    if (status != nx_http::StatusCode::ok && status != nx_http::StatusCode::partialContent)
        return status;

    result.clear();
    while (!httpClient.eof())
        result.append(httpClient.fetchMessageBodyBuffer());

    if (!httpClient.isValid())
    {
        result.clear();
        return nx_http::StatusCode::internalServerError;
    }

    resultContentType = kOctetStreamContentType;

    if (useDownloader)
        downloader->writeFileChunk(fileName, chunkIndex, result);

    return nx_http::StatusCode::ok;
}

int Helper::handleUploadChunk(
    const QString& fileName,
    int chunkIndex,
    const QByteArray& body,
    const QByteArray& contentType)
{
    if (contentType != kOctetStreamContentType)
    {
        return makeError(
            nx_http::StatusCode::badRequest,
            QnRestResult::CantProcessRequest,
            lit("Only %1 Content-Type is supported.").arg(
                QLatin1String(kOctetStreamContentType)));
    }

    const auto errorCode = downloader->writeFileChunk(fileName, chunkIndex, body);
    if (errorCode != ResultCode::ok)
        return makeDownloaderError(errorCode);

    return nx_http::StatusCode::ok;
}

int Helper::handleStatus(const QString& fileName)
{
    if (!fileName.isEmpty())
    {
        const auto& fileInfo = downloader->fileInformation(fileName);
        if (!fileInfo.isValid())
            return makeFileError(fileName);

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

int Helper::makeError(
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

int Helper::makeInvalidParameterError(
    const QString& parameter, const QnRestResult::Error& error)
{
    return makeError(nx_http::StatusCode::invalidParameter, error, parameter);
}

int Helper::makeFileError(const QString& fileName)
{
    return makeError(
        nx_http::StatusCode::badRequest,
        QnRestResult::CantProcessRequest,
        lit("%1: file does not exist.").arg(fileName));
}

int Helper::makeDownloaderError(ResultCode errorCode)
{
    return makeError(
        nx_http::StatusCode::internalServerError,
        QnRestResult::CantProcessRequest,
        lit("DistributedFileDownloader returned error: %1").arg(
            QnLexical::serialized(errorCode)));
}

bool Helper::hasDownloader() const
{
    return downloader != nullptr;
}

} // namespace

int QnDownloadsRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* /*processor*/)
{
    Request request(path);
    if (!request.isValid())
        return nx_http::StatusCode::badRequest;

    Helper helper(this, params, result, resultContentType);
    if (!verifyRelativePath(request.fileName))
    {
        return helper.makeError(
            nx_http::StatusCode::badRequest,
            QnRestResult::InvalidParameter,
            "File name in not a valid relative path.");
    }

    if (!helper.hasDownloader())
    {
        return helper.makeError(
            nx_http::StatusCode::internalServerError,
            QnRestResult::CantProcessRequest,
            "DistributedFileDownloader is not initialized.");
    }

    switch (request.subject)
    {
        case Request::Subject::chunk:
            return helper.handleDownloadChunk(request.fileName, request.chunkIndex);
        case Request::Subject::checksums:
            return helper.handleGetChunkChecksums(request.fileName);
        case Request::Subject::status:
            return helper.handleStatus(request.fileName);
        default:
            return nx_http::StatusCode::badRequest;
    }
}

int QnDownloadsRestHandler::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& bodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* /*processor*/)
{
    Request request(path);
    if (!request.isValid())
        return nx_http::StatusCode::badRequest;

    Helper helper(this, params, result, resultContentType);
    if (!verifyRelativePath(request.fileName))
    {
        return helper.makeError(
            nx_http::StatusCode::badRequest,
            QnRestResult::InvalidParameter,
            "File name in not a valid relative path.");
    }

    if (!helper.hasDownloader())
    {
        return helper.makeError(
            nx_http::StatusCode::internalServerError,
            QnRestResult::CantProcessRequest,
            "DistributedFileDownloader is not initialized.");
    }

    switch (request.subject)
    {
        case Request::Subject::file:
            return helper.handleAddDownload(request.fileName);
        case Request::Subject::chunk:
            return helper.handleUploadChunk(
                request.fileName, request.chunkIndex, body, bodyContentType);
        default:
            return nx_http::StatusCode::badRequest;
    }

    return nx_http::StatusCode::ok;
}

int QnDownloadsRestHandler::executePut(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& bodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    return executePost(path, params, body, bodyContentType, result, resultContentType, processor);
}

int QnDownloadsRestHandler::executeDelete(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* /*processor*/)
{
    Request request(path);
    if (!request.isValid() || request.subject != Request::Subject::file)
        return nx_http::StatusCode::badRequest;

    Helper helper(this, params, result, resultContentType);
    if (!verifyRelativePath(request.fileName))
    {
        return helper.makeError(
            nx_http::StatusCode::badRequest,
            QnRestResult::InvalidParameter,
            "File name in not a valid relative path.");
    }

    if (!helper.hasDownloader())
    {
        return helper.makeError(
            nx_http::StatusCode::internalServerError,
            QnRestResult::CantProcessRequest,
            "DistributedFileDownloader is not initialized.");
    }

    return helper.handleRemoveDownload(request.fileName);
}
