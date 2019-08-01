#include "downloads_rest_handler.h"

#include <chrono>

#include <nx/network/http/http_types.h>
#include <nx/network/http/http_client.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include <rest/server/json_rest_result.h>
#include <rest/helpers/request_helpers.h>
#include <media_server/media_server_module.h>
#include <nx/utils/file_system.h>
#include <nx/fusion/serialization/lexical.h>
#include <recorder/storage_manager.h>

using namespace std::chrono;

using nx::vms::common::p2p::downloader::Downloader;
using nx::vms::common::p2p::downloader::FileInformation;
using nx::vms::common::p2p::downloader::ResultCode;

namespace nx::vms::server::rest::handlers {

namespace {

static const QByteArray kJsonContentType("application/json");
static const QByteArray kOctetStreamContentType("application/octet-stream");
static const int kMaxChunkSize = 10 * 1024 * 1024;

struct Request
{
    enum class Subject
    {
        unknown,
        file,
        chunk,
        status,
        checksums,
    };

    QString fileName;
    QString url;
    int expectedSize = -1;
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
        Downloads* handler,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& resultContentType);

    int handleAddDownload(const QString& fileName);
    int handleAddUpload(const QString& fileName);
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
    /**
     * Generate a response with parameter error.
     * @param parameter - name of parater
     * @param error - generic error code
     * @returns - http status code
     */
    int makeInvalidParameterError(
        const QString& parameter,
        const QnRestResult::Error& error = QnRestResult::InvalidParameter);
    /**
     * Generate a response with file error.
     * @param fileName - path to a file with a problem
     * @param at - code location, that got this sort of an error
     * @returns - http status code
     */
    int makeFileError(const QString& fileName, const QString& at);
    int makeDownloaderError(ResultCode errorCode);

    bool hasDownloader() const;

    ResultCode addFile(const FileInformation& fileInfo);

private:
    Downloads* handler;
    Downloader* const downloader;
    const QnRequestParamList& params;
    QByteArray& result;
    QByteArray& resultContentType;
};

Helper::Helper(Downloads* handler,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType)
    :
    handler(handler),
    downloader(handler->serverModule()->p2pDownloader()),
    params(params),
    result(result),
    resultContentType(resultContentType)
{
}

int Helper::handleAddDownload(const QString& fileName)
{
    NX_VERBOSE(handler, "Creating download: %1", fileName);
    FileInformation fileInfo(fileName);

    const auto sizeString = params.value("size");
    if (!sizeString.isEmpty())
    {
        bool ok;
        fileInfo.size = sizeString.toLongLong(&ok);
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
        fileInfo.url = nx::utils::Url(urlString);
        if (!fileInfo.url.isValid())
            return makeInvalidParameterError("url");
    }

    const auto peerPolicyString = params.value("peerPolicy");
    if (!peerPolicyString.isEmpty())
    {
        bool deserialized = false;
        fileInfo.peerPolicy = QnLexical::deserialized<FileInformation::PeerSelectionPolicy>(
            peerPolicyString,
            FileInformation::PeerSelectionPolicy::none,
            &deserialized);
    }

    const auto errorCode = addFile(fileInfo);
    if (errorCode != ResultCode::ok)
        return makeDownloaderError(errorCode);

    NX_VERBOSE(handler, "Download created: %1", fileName);
    return nx::network::http::StatusCode::ok;
}

int Helper::handleAddUpload(const QString& fileName)
{
    NX_VERBOSE(handler, "Creating upload: %1", fileName);
    FileInformation fileInfo(fileName);

    const auto sizeString = params.value("size");
    if (!sizeString.isEmpty())
    {
        bool ok;
        fileInfo.size = sizeString.toLongLong(&ok);
        if (!ok || fileInfo.size <= 0)
            return makeInvalidParameterError("size");
    }
    else
    {
        return makeInvalidParameterError("size", QnRestResult::MissingParameter);
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
    else
    {
        return makeInvalidParameterError("md5", QnRestResult::MissingParameter);
    }

    const auto chunkSizeString = params.value("chunkSize");
    if (!chunkSizeString.isEmpty())
    {
        bool ok;
        fileInfo.chunkSize = chunkSizeString.toLongLong(&ok);
        if(!ok || fileInfo.chunkSize > kMaxChunkSize)
            return makeInvalidParameterError("chunkSize");
    }
    else
    {
        return makeInvalidParameterError("chunkSize", QnRestResult::MissingParameter);
    }

    const auto ttlString = params.value("ttl");
    if (!ttlString.isEmpty())
    {
        bool ok;
        fileInfo.ttl = ttlString.toLongLong(&ok);
        if (!ok)
            return makeInvalidParameterError("ttl");
    }

    const bool recreate = params.value("recreate") == "true";

    fileInfo.status = FileInformation::Status::uploading;
    auto errorCode = addFile(fileInfo);

    switch (errorCode)
    {
        case ResultCode::fileAlreadyExists:
        {
            const auto info = downloader->fileInformation(fileInfo.name);
            NX_ASSERT(info.isValid());
            if (info.status != FileInformation::Status::downloaded && recreate)
            {
                downloader->deleteFile(fileInfo.name);
                errorCode = addFile(fileInfo);
                if (errorCode != ResultCode::ok)
                    return makeDownloaderError(errorCode);
                break;
            }
            return makeDownloaderError(errorCode);
        }
        case ResultCode::ok:
            break;
        default:
            return makeDownloaderError(errorCode);
    }

    QnJsonRestResult restResult;
    restResult.setReply(errorCode);
    QnFusionRestHandlerDetail::serialize(restResult, result, resultContentType, Qn::JsonFormat, false);
    NX_VERBOSE(handler, "Upload created: %1", fileName);
    return nx::network::http::StatusCode::ok;
}

int Helper::handleRemoveDownload(const QString& fileName)
{
    NX_VERBOSE(handler, "Removing download: %1", fileName);

    const bool deleteData = params.value("deleteData", "true") != "false";

    const auto errorCode = downloader->deleteFile(fileName, deleteData);
    if (errorCode != ResultCode::ok)
        return makeDownloaderError(errorCode);

    NX_VERBOSE(handler, "Download removed: %1", fileName);
    return nx::network::http::StatusCode::ok;
}

int Helper::handleGetChunkChecksums(const QString& fileName)
{
    NX_VERBOSE(handler, "Checksums requested for: %1", fileName);

    const auto& fileInfo = downloader->fileInformation(fileName);
    if (!fileInfo.isValid())
        return makeFileError(fileName, "handleGetChunkChecksums");

    const auto& checksums = downloader->getChunkChecksums(fileInfo.name);

    QnFusionRestHandlerDetail::serializeJsonRestReply(
        checksums, params, result, resultContentType, QnRestResult());

    NX_VERBOSE(handler, "Read checksums for %1 chunks of %2", checksums.size(), fileName);
    return nx::network::http::StatusCode::ok;
}

int Helper::handleDownloadChunk(const QString& fileName, int chunkIndex)
{
    NX_VERBOSE(handler, "Chunk %1 requested for: %2", chunkIndex, fileName);

    if (params.value("fromInternet", "false") == "true")
        return handleDownloadChunkFromInternet(fileName, chunkIndex);

    QByteArray data;
    const auto errorCode = downloader->readFileChunk(fileName, chunkIndex, data);
    if (errorCode != ResultCode::ok)
        return makeDownloaderError(errorCode);

    result = data;
    resultContentType = kOctetStreamContentType;

    NX_VERBOSE(handler, "Successfully returned chunk %1 for: %2", chunkIndex, fileName);
    return nx::network::http::StatusCode::ok;
}

int Helper::handleDownloadChunkFromInternet(const QString& fileName, int chunkIndex)
{
    NX_VERBOSE(handler, "Chunk %1 requested for: %2 from Internet", chunkIndex, fileName);

    const nx::utils::Url url = params.value("url");
    if (url.isEmpty())
        return makeInvalidParameterError("url", QnRestResult::MissingParameter);
    else if (!url.isValid())
        return makeInvalidParameterError("url", QnRestResult::InvalidParameter);

    const QString chunkSizeString = params.value("chunkSize");
    if (chunkSizeString.isEmpty())
        return makeInvalidParameterError("chunkSize", QnRestResult::MissingParameter);
    const qint64 chunkSize = chunkSizeString.toLongLong();
    if (chunkSize <= 0 || chunkSize > kMaxChunkSize)
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
            NX_VERBOSE(handler, "Chunk %1 was read from the local file %2", chunkIndex, fileName);
            return nx::network::http::StatusCode::ok;
        }
    }

    constexpr milliseconds kDownloadRequestTimeout = 1min;
    nx::network::http::HttpClient httpClient;
    httpClient.setResponseReadTimeout(kDownloadRequestTimeout);
    httpClient.setSendTimeout(kDownloadRequestTimeout);
    httpClient.setMessageBodyReadTimeout(kDownloadRequestTimeout);

    const qint64 pos = chunkIndex * chunkSize;
    const qint64 end = pos + chunkSize - 1;
    httpClient.addAdditionalHeader("Range",
        QStringLiteral("bytes=%1-%2").arg(pos).arg(end).toLatin1());

    NX_VERBOSE(handler, "Requesting data [%1-%2] from %3 for chunk %4 of %5",
        pos, end, url, chunkIndex, fileName);

    const auto requestStartTime = steady_clock::now();

    if (!httpClient.doGet(url) || !httpClient.response())
        return nx::network::http::StatusCode::internalServerError;

    const auto status = httpClient.response()->statusLine.statusCode;

    if (status != nx::network::http::StatusCode::ok
        && status != nx::network::http::StatusCode::partialContent)
    {
        NX_VERBOSE(handler, "Request failed for %1 with status %2", fileName, status);
        return status;
    }

    result.clear();
    while (!httpClient.eof())
        result.append(httpClient.fetchMessageBodyBuffer());

    if (!httpClient.isValid())
    {
        result.clear();
        NX_VERBOSE(handler, "Request failed for %1", fileName);
        return nx::network::http::StatusCode::internalServerError;
    }

    NX_VERBOSE(handler, "Successfully downloaded chunk %1 of %2 in %3",
        chunkIndex, fileName, steady_clock::now() - requestStartTime);

    resultContentType = kOctetStreamContentType;

    if (useDownloader)
    {
        NX_VERBOSE(handler, "Writing chunk %1 of %2", chunkIndex, fileName);
        downloader->writeFileChunk(fileName, chunkIndex, result);
    }

    NX_VERBOSE(handler, "Successfully returned chunk %1 for: %2", chunkIndex, fileName);
    return nx::network::http::StatusCode::ok;
}

int Helper::handleUploadChunk(
    const QString& fileName,
    int chunkIndex,
    const QByteArray& body,
    const QByteArray& contentType)
{
    NX_VERBOSE(handler, "Upload requested for chunk %1 of %2", chunkIndex, fileName);

    if (contentType != kOctetStreamContentType)
    {
        return makeError(
            nx::network::http::StatusCode::badRequest,
            QnRestResult::CantProcessRequest,
            QString("Only %1 Content-Type is supported.").arg(
                kOctetStreamContentType.constData()));
    }

    const auto errorCode = downloader->writeFileChunk(fileName, chunkIndex, body);
    if (errorCode != ResultCode::ok)
        return makeDownloaderError(errorCode);

    NX_VERBOSE(handler, "Successfully wrote chunk %1 of %2", chunkIndex, fileName);
    return nx::network::http::StatusCode::ok;
}

int Helper::handleStatus(const QString& fileName)
{
    if (!fileName.isEmpty())
    {
        NX_VERBOSE(handler, "Status requested for %1", fileName);

        const auto& fileInfo = downloader->fileInformation(fileName);
        if (!fileInfo.isValid())
            return makeFileError(fileName, "handleStatus");

        QnFusionRestHandlerDetail::serializeJsonRestReply(
            fileInfo, params, result, resultContentType, QnRestResult());

        NX_VERBOSE(handler, "Returned status for %1: %2", fileName, fileInfo.status);
    }
    else
    {
        NX_VERBOSE(handler, "Status requested for all files");

        QList<FileInformation> infoList;
        for (const auto& fileName: downloader->files())
        {
            const auto& fileInfo = downloader->fileInformation(fileName);
            infoList.append(fileInfo);

            NX_VERBOSE(handler, "Returned status for %1: %2", fileName, fileInfo.status);
        }

        QnFusionRestHandlerDetail::serializeJsonRestReply(
            infoList, params, result, resultContentType, QnRestResult());
    }

    return nx::network::http::StatusCode::ok;
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
    return makeError(nx::network::http::StatusCode::unprocessableEntity, error, parameter);
}

int Helper::makeFileError(const QString& fileName, const QString& at)
{
    NX_ERROR(handler, "Bad file name: %1", fileName);
    return makeError(
        nx::network::http::StatusCode::badRequest,
        QnRestResult::CantProcessRequest,
        QString("%1(%2): file does not exist.").arg(at, fileName));
}

int Helper::makeDownloaderError(ResultCode errorCode)
{
    QnJsonRestResult restResult;
    const auto errorMessage = QString("DistributedFileDownloader returned error: %1").arg(
        QnLexical::serialized(errorCode));
    NX_ERROR(handler, "makeDownloaderError: %1", errorMessage);
    restResult.setError(QnRestResult::CantProcessRequest, errorMessage);
    restResult.setReply(errorCode);

    QnFusionRestHandlerDetail::serialize(restResult, result, resultContentType, Qn::JsonFormat, false);
    // It will look like:
    // {"error":"3","errorString":"DistributedFileDownloader returned error: fileAlreadyExists","reply":"fileAlreadyExists"}
    return nx::network::http::StatusCode::internalServerError;
}

ResultCode Helper::addFile(
    const FileInformation& fileInfo)
{
    auto serverModule = handler->serverModule();
    ResultCode errorCode = downloader->addFile(fileInfo);
    if (errorCode == ResultCode::noFreeSpace) {
        serverModule->normalStorageManager()->clearSpaceForFile(downloader->filePath(fileInfo.name), fileInfo.size);
        errorCode = downloader->addFile(fileInfo);
    }
    return errorCode;
}

bool Helper::hasDownloader() const
{
    return downloader != nullptr;
}

boost::optional<int> hasError(const Request& request, Helper& helper)
{
    if (!request.isValid())
        return nx::network::http::StatusCode::badRequest;

    if (!nx::utils::file_system::isRelativePathSafe(request.fileName))
    {
        return helper.makeError(
            nx::network::http::StatusCode::badRequest,
            QnRestResult::InvalidParameter,
            "File name in not a valid relative path.");
    }

    if (!helper.hasDownloader())
    {
        return helper.makeError(
            nx::network::http::StatusCode::internalServerError,
            QnRestResult::CantProcessRequest,
            "DistributedFileDownloader is not initialized.");
    }

    return boost::none;
}

} // namespace

Downloads::Downloads(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

int Downloads::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* /*processor*/)
{
    Request request(path);
    Helper helper(this, params, result, resultContentType);
    if (const auto returnCode = hasError(request, helper))
        return *returnCode;

    switch (request.subject)
    {
        case Request::Subject::chunk:
            return helper.handleDownloadChunk(request.fileName, request.chunkIndex);
        case Request::Subject::checksums:
            return helper.handleGetChunkChecksums(request.fileName);
        case Request::Subject::status:
            return helper.handleStatus(request.fileName);
        default:
            return nx::network::http::StatusCode::badRequest;
    }
}

int Downloads::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& body,
    const QByteArray& bodyContentType,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* /*processor*/)
{
    Request request(path);
    Helper helper(this, params, result, resultContentType);
    if (const auto returnCode = hasError(request, helper))
        return *returnCode;

    switch (request.subject)
    {
        case Request::Subject::file:
            if (params.contains("upload"))
                return helper.handleAddUpload(request.fileName);
            else
                return helper.handleAddDownload(request.fileName);
        case Request::Subject::chunk:
            return helper.handleUploadChunk(
                request.fileName, request.chunkIndex, body, bodyContentType);
        default:
            return nx::network::http::StatusCode::badRequest;
    }
}

int Downloads::executePut(
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

int Downloads::executeDelete(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* /*processor*/)
{
    Request request(path);
    Helper helper(this, params, result, resultContentType);
    if (const auto returnCode = hasError(request, helper))
        return *returnCode;

    return helper.handleRemoveDownload(request.fileName);
}

} // namespace nx::vms::server::rest::handlers
