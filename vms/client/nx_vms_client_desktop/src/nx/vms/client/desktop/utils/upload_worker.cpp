#include "upload_worker.h"
#include "server_request_storage.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>

#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/common/p2p/downloader/file_information.h>
#include <nx/vms/common/p2p/downloader/result_code.h>

namespace nx::vms::client::desktop {

struct UploadWorker::Private
{
    UploadWorker* const q;

    Private(UploadWorker* parent, const QnMediaServerResourcePtr& server):
        q(parent),
        server(server),
        requests(server)
    {
    }

    rest::QnConnectionPtr connection() const
    {
        return server->restConnection();
    }

    UploadState upload;
    QnMediaServerResourcePtr server;
    QSharedPointer<QFile> file;
    QByteArray md5;
    int chunkSize = 1024 * 1024;
    int currentChunk = 0;
    int maxSubsequentChunksToUpload = 10;
    int subsequentChunksToUploadLeft = -1;
    QBitArray uploadedChunks;
    QnMutex mutex;

    QFuture<QByteArray> md5Future;
    QFutureWatcher<QByteArray> md5FutureWatcher;
    ServerRequestStorage requests;

public:
    void getAvailableChunks();
    void handleGotAvailableChunks(
        const common::p2p::downloader::FileInformation& fileInformation);
};

void UploadWorker::Private::getAvailableChunks()
{
    NX_ASSERT(!upload.uploadAllChunks);

    NX_VERBOSE(this, "Getting available chunks for %1", upload.destination);

    const auto callback = nx::utils::guarded(q,
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            {
                QnMutexLocker lock(&mutex);
                requests.releaseHandle(handle);
            }

            if (success)
            {
                const auto& fileInfo =
                    result.deserialized<common::p2p::downloader::FileInformation>();
                handleGotAvailableChunks(fileInfo);
            }
            else
            {
                handleGotAvailableChunks({});
            }
        });

    requests.storeHandle(connection()->fileDownloadStatus(
        upload.destination,
        callback,
        q->thread()));
}

void UploadWorker::Private::handleGotAvailableChunks(
    const common::p2p::downloader::FileInformation& fileInformation)
{
    NX_VERBOSE(this, "Got available chunks for %1: %2",
        upload.destination, fileInformation.isValid());

    if (fileInformation.isValid() && !fileInformation.downloadedChunks.isEmpty()
        && uploadedChunks.size() == fileInformation.downloadedChunks.size())
    {
        uploadedChunks = fileInformation.downloadedChunks;
    }

    subsequentChunksToUploadLeft = maxSubsequentChunksToUpload;
    q->handleUpload();
}

UploadWorker::UploadWorker(
    const QnMediaServerResourcePtr& server,
    UploadState& config,
    QObject* parent)
    :
    QObject(parent),
    d(new Private(this, server))
{
    NX_ASSERT(server);

    d->upload = config;
    d->upload.uuid = server->getId();
    d->upload.source = config.source;
    d->upload.destination = config.destination;
    d->upload.ttl = config.ttl;

    d->file.reset(new QFile(config.source));

    connect(&d->md5FutureWatcher, &QFutureWatcherBase::finished,
        this, &UploadWorker::handleMd5Calculated);
}

UploadWorker::~UploadWorker()
{
    handleStop();
}

void UploadWorker::start()
{
    NX_ASSERT(d->upload.status == UploadState::Initial);

    d->upload.id = "tmp-" + QnUuid::createUuid().toSimpleString();
    QFileInfo info(d->file->fileName());
    if (!info.suffix().isEmpty())
        d->upload.id += L'.' + info.suffix();

    if (d->upload.destination.isEmpty())
        d->upload.destination = d->upload.id;

    NX_INFO(this, "Start uploading %1", d->upload.destination);

    if (!d->file->open(QIODevice::ReadOnly))
    {
        d->upload.status = UploadState::Error;
        d->upload.errorMessage = tr("Could not open file \"%1\"").arg(d->file->fileName());
        NX_ERROR(this, "Cannot open %1", d->upload.destination);
        return;
    }

    d->upload.size = d->file->size();
    d->uploadedChunks.resize((int) ((d->upload.size + d->chunkSize - 1) / d->chunkSize));

    d->upload.status = UploadState::CalculatingMD5;

    emitProgress();

    QSharedPointer<QFile> fileCopy = d->file;
    d->md5Future = QtConcurrent::run(
        [fileCopy]()
        {
            nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Md5);
            if (hash.addData(fileCopy.data()))
                return hash.result();
            return QByteArray();
        }
    );
    d->md5FutureWatcher.setFuture(d->md5Future);
}

void UploadWorker::cancel()
{
    NX_INFO(this, "Cancelling upload for %1", d->upload.destination);

    UploadState::Status status = d->upload.status;
    if (status == UploadState::Initial || status == UploadState::Done ||
        status == UploadState::Error || status == UploadState::Canceled)
    {
        return;
    }

    handleStop();
    d->upload.status = UploadState::Canceled;
    /* We don't emit signals here as canceling is also a way of silencing the worker. */
}

UploadState UploadWorker::state() const
{
    return d->upload;
}

void UploadWorker::emitProgress()
{
    emit progress(d->upload);
}

void UploadWorker::handleStop()
{
    NX_INFO(this, "Stopping upload for %1", d->upload.destination);

    d->md5FutureWatcher.disconnect(this);
    d->requests.cancelAllRequests();

    UploadState::Status status = d->upload.status;
    if (status == UploadState::CreatingUpload
        || status == UploadState::Uploading
        || status == UploadState::Checking)
    {
        d->connection()->removeFileDownload(d->upload.id, /*deleteData*/ true);
    }
}

void UploadWorker::handleError(const QString& message)
{
    handleStop();

    d->upload.status = UploadState::Error;
    d->upload.errorMessage = message;
    emitProgress();
}

void UploadWorker::handleMd5Calculated()
{
    QByteArray md5 = d->md5Future.result();
    if (md5.isEmpty())
    {
        NX_ERROR(this, "Cannot calculate MD5 for %1", d->file->fileName());

        handleError(tr("Could not calculate md5 for file \"%1\"").arg(d->file->fileName()));
        return;
    }

    d->md5 = md5;
    d->upload.status = UploadState::CreatingUpload;

    emitProgress();

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const QnJsonRestResult & result)
        {
            {
                QnMutexLocker lock(&d->mutex);
                d->requests.releaseHandle(handle);
            }
            RemoteResult code = RemoteResult::ok;
            if (!success)
            {
                if (result.reply.isNull())
                    code = RemoteResult::ioError;
                else
                    code = result.deserialized<RemoteResult>();
            }

            QMetaObject::invokeMethod(this, [this, success, code, error=result.errorString]()
                {
                    handleFileUploadCreated(success, code, error);
                });
            //handleFileUploadCreated(success, code, result.errorString);
        });

    auto handle = d->connection()->addFileUpload(
        d->upload.destination,
        d->upload.size,
        d->chunkSize,
        d->md5.toHex(),
        d->upload.ttl,
        d->upload.recreateFile,
        callback, thread());
    // We have a race condition around this handle.
    d->requests.storeHandle(handle);
}

void UploadWorker::handleFileUploadCreated(bool success, RemoteResult code, QString error)
{
    NX_VERBOSE(this, "Upload for %1 was created: %2, %3", d->upload.destination, success, code);

    if (!success)
    {
        switch (code)
        {
            case RemoteResult::ok:
                // Should not be here. success should be true
                NX_ASSERT(false);
                [[fallthrough]];
            case RemoteResult::fileAlreadyExists:
                d->upload.status = UploadState::Uploading;
                handleUpload();
                break;
            case RemoteResult::fileAlreadyDownloaded:
                d->upload.status = UploadState::Done;
                emitProgress();
                break;
            case RemoteResult::ioError:
            case RemoteResult::fileDoesNotExist:
            case RemoteResult::invalidChecksum:
            case RemoteResult::invalidFileSize:
            case RemoteResult::invalidChunkIndex:
            case RemoteResult::invalidChunkSize:
            case RemoteResult::noFreeSpace:
                d->upload.status = UploadState::Error;
                d->upload.errorMessage = error;
                handleError(tr("Could not create upload on the server side: %1").arg(error));
                break;
        }
    }
    else
    {
        d->upload.status = UploadState::Uploading;
        handleUpload();
    }
}

void UploadWorker::handleUpload()
{
    while (d->currentChunk < d->uploadedChunks.size() && d->uploadedChunks[d->currentChunk])
        ++d->currentChunk;

    if (d->currentChunk >= d->uploadedChunks.size())
    {
        handleAllUploaded();
        return;
    }

    emitProgress();

    if (!d->upload.uploadAllChunks)
    {
        if (d->subsequentChunksToUploadLeft <= 0)
        {
            d->getAvailableChunks();
            return;
        }
        --d->subsequentChunksToUploadLeft;
    }

    NX_VERBOSE(this, "Uploading chunk %1 for %2", d->currentChunk, d->upload.destination);

    bool seekOk = d->file->seek(static_cast<qint64>(d->chunkSize) * d->currentChunk);
    QByteArray bytes = d->file->read(d->chunkSize);
    if (!seekOk || bytes.isEmpty())
    {
        NX_ERROR(this, "Cannot read chunk %1 [offset: %2, size: %3] from %4: %5",
            d->currentChunk, d->chunkSize * d->currentChunk, d->chunkSize, d->upload.destination,
            d->file->errorString());
        handleError(d->file->errorString());
        return;
    }

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
        {
            {
                QnMutexLocker lock(&d->mutex);
                d->requests.releaseHandle(handle);
            }
            handleChunkUploaded(success);
        });

    d->requests.storeHandle(d->connection()->uploadFileChunk(
        d->upload.destination,
        d->currentChunk,
        bytes,
        callback,
        thread()));
}

void UploadWorker::handleChunkUploaded(bool success)
{
    NX_VERBOSE(this, "Chunk %1 was uploaded for %2: %3",
        d->currentChunk, d->upload.destination, success);

    if (!success)
    {
        handleError(tr("Could not upload file chunk to the server"));
        return;
    }

    d->uploadedChunks[d->currentChunk] = true;
    d->upload.uploaded = std::min<qint64>(
        d->uploadedChunks.count(true) * d->chunkSize, d->upload.size);

    ++d->currentChunk;

    handleUpload();
}

void UploadWorker::handleAllUploaded()
{
    NX_INFO(this, "Upload for %1 is done. Checking...", d->upload.destination);

    d->upload.status = UploadState::Checking;

    emitProgress();

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
        {
            {
                QnMutexLocker lock(&d->mutex);
                d->requests.releaseHandle(handle);
            }
            using namespace nx::vms::common::p2p::downloader;
            FileInformation info = result.deserialized<FileInformation>();
            const bool fileStatusOk = info.status == FileInformation::Status::downloaded;
            handleCheckFinished(success, fileStatusOk);
        });

    d->requests.storeHandle(d->connection()->fileDownloadStatus(
        d->upload.destination,
        callback, thread()));
}

void UploadWorker::handleCheckFinished(bool success, bool ok)
{
    NX_INFO(this, "The final check finished: %1", success && ok);

    if (!success)
    {
        handleError(tr("Could not check uploaded file on the server"));
        return;
    }

    if (!ok)
    {
        handleError(tr("File was corrupted while being uploaded to the server"));
        return;
    }

    d->upload.status = UploadState::Done;

    emitProgress();
}

} // namespace nx::vms::client::desktop
