// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "upload_worker.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFutureWatcher>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/random.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/common/p2p/downloader/file_information.h>
#include <nx/vms/common/p2p/downloader/result_code.h>
#include <utils/common/delayed.h>

#include "server_request_storage.h"

namespace nx::vms::client::desktop {

using nx::vms::common::p2p::downloader::FileInformation;

struct UploadWorker::Private: public nx::vms::client::core::RemoteConnectionAware
{
    UploadWorker* const q;

    Private(UploadWorker* parent, const QnMediaServerResourcePtr& server):
        q(parent),
        server(server)
    {
    }

    UploadState upload;
    QnMediaServerResourcePtr server;
    QSharedPointer<QFile> file;
    QByteArray md5;
    int chunkSize = 1024 * 1024;
    int maxSubsequentChunksToUpload = 10;
    int subsequentChunksToUploadLeft = -1;
    QBitArray uploadedChunks;
    nx::Mutex mutex;

    QFuture<QByteArray> md5Future;
    QFutureWatcher<QByteArray> md5FutureWatcher;
    ServerRequestStorage requests;

public:
    void getAvailableChunks();
    void handleGotAvailableChunks(
        const common::p2p::downloader::FileInformation& fileInformation);
    int selectNextChunkToUpload() const;
    int selectRandomChunkToUpload() const;
    int selectChunkToUpload() const;

    QByteArray readChunk(int chunkIndex);
};

void UploadWorker::Private::getAvailableChunks()
{
    if (!connection())
        return;

    NX_ASSERT(!upload.uploadAllChunks);

    NX_VERBOSE(this, "Getting available chunks for %1", upload.destination);

    const auto callback = nx::utils::guarded(q,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            {
                NX_MUTEX_LOCKER lock(&mutex);
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

    requests.storeHandle(connectedServerApi()->fileDownloadStatus(
        server->getId(),
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

int UploadWorker::Private::selectNextChunkToUpload() const
{
    int chunk = 0;
    while (chunk < uploadedChunks.size() && uploadedChunks[chunk])
        ++chunk;

    return chunk < uploadedChunks.size() ? chunk : -1;
}

int UploadWorker::Private::selectRandomChunkToUpload() const
{
    const int chunksCount = uploadedChunks.size();
    const int randomChunk = utils::random::number(0, chunksCount - 1);

    for (int i = randomChunk; i < chunksCount; ++i)
    {
        if (!uploadedChunks[i])
            return i;
    }
    for (int i = 0; i < randomChunk; ++i)
    {
        if (!uploadedChunks[i])
            return i;
    }

    return -1;
}

int UploadWorker::Private::selectChunkToUpload() const
{
    if (upload.uploadAllChunks)
        return selectNextChunkToUpload();
    return selectRandomChunkToUpload();
}

QByteArray UploadWorker::Private::readChunk(int chunkIndex)
{
    if (file->seek(static_cast<qint64>(chunkSize) * chunkIndex))
    {
        const QByteArray& data = file->read(chunkSize);
        if (!data.isEmpty())
            return data;
    }

    NX_ERROR(this, "Cannot read chunk %1 [offset: %2, size: %3] from %4: %5",
        chunkIndex, chunkSize * chunkIndex, chunkSize, upload.destination,
        file->errorString());
    q->handleError(file->errorString());
    return {};
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

    d->upload.id = "tmp-" + nx::Uuid::createUuid().toSimpleString();
    QFileInfo info(d->file->fileName());
    if (!info.suffix().isEmpty())
        d->upload.id += '.' + info.suffix();

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


void UploadWorker::createUpload()
{
    if (!d->connection())
        return;

    d->upload.status = UploadState::CreatingUpload;

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            {
                NX_MUTEX_LOCKER lock(&d->mutex);
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

    auto handle = d->connectedServerApi()->addFileUpload(
        d->server->getId(),
        d->upload.destination,
        d->upload.size,
        d->chunkSize,
        d->md5.toHex(),
        d->upload.ttl,
        d->upload.recreateFile,
        callback,
        thread());
    // We have a race condition around this handle.
    d->requests.storeHandle(handle);
}

void UploadWorker::handleWaitForFileOnServer(
    bool success, int handle, const nx::network::rest::JsonResult& result)
{
    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        d->requests.releaseHandle(handle);
    }

    if (d->upload.status != UploadState::WaitingFileOnServer)
        return;
    if (success)
    {
        auto information = result.deserialized<FileInformation>();
        switch(information.status)
        {
            case FileInformation::Status::notFound:
                // Try again later.
                executeDelayedParented([this] { checkRemoteFile(); }, 1000, this);
                break;
            case FileInformation::Status::corrupted:
                // try to recreate if allowed
                if (d->upload.recreateFile)
                {
                    NX_WARNING(this, "Remote file \"%1\" was corrupted. Recreating its contents",
                        d->file->fileName());
                    createUpload();
                }
                else
                {
                    handleError(tr("Remote file \"%1\" is corrupted").arg(d->file->fileName()));
                }
                break;
            case FileInformation::Status::downloaded:
            case FileInformation::Status::downloading:
            case FileInformation::Status::uploading:
                if (information.md5 == d->md5)
                {
                    if (information.status == FileInformation::Status::downloaded)
                    {
                        NX_DEBUG(this, "Remote file \"%1\" is already downloaded. "
                            "No need to upload anything.", d->file->fileName());
                        handleCheckFinished(true, true);
                    }
                    else
                    {
                        NX_DEBUG(this, "Remote file \"%1\" has appeared. I can start uploading.",
                            d->file->fileName());
                        // TODO: We can reuse information about current chunks
                        // and save some time requesting it again.
                        d->upload.status = UploadState::Uploading;
                        emitProgress();
                        handleUpload();
                    }
                }
                else if (d->upload.recreateFile)
                {
                    NX_WARNING(this, "There is md5 mismatch with server file \"%1\" "
                        "and I am told to recreate it.", d->file->fileName());
                    createUpload();
                }
                else
                {
                    handleError(tr("Server already has this file \"%1\"").arg(
                        d->file->fileName()));
                }
                break;
        }
    }
    else
    {
        // Try again later.
        executeDelayedParented([this] { checkRemoteFile(); }, 1000, this);
    }
}

void UploadWorker::checkRemoteFile()
{
    // Client can be disconnected while the event is queued.
    if (!d->connection())
        return;

    d->upload.status = UploadState::WaitingFileOnServer;
    auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            handleWaitForFileOnServer(success, handle, result);
        });
    auto handle = d->connectedServerApi()->fileDownloadStatus(
        d->server->getId(),
        d->upload.destination,
        callback,
        thread());
    d->requests.storeHandle(handle);
}

void UploadWorker::handleStop()
{
    NX_INFO(this, "Stopping upload for %1", d->upload.destination);

    d->md5FutureWatcher.disconnect(this);
    d->requests.cancelAllRequests();

    if (!d->connection())
        return;

    UploadState::Status status = d->upload.status;
    if (status == UploadState::CreatingUpload
        || status == UploadState::Uploading
        || status == UploadState::Checking)
    {
        d->connectedServerApi()->removeFileDownload(
            d->server->getId(),
            d->upload.id,
            /*deleteData*/ true);
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

    if (!d->upload.allowFileCreation)
        checkRemoteFile();
    else
        createUpload();

    emitProgress();
}

void UploadWorker::handleFileUploadCreated(bool success, RemoteResult code, QString error)
{
    NX_DEBUG(this, "Upload for %1 was created: %2, %3", d->upload.destination, success, code);

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
    const int chunk = d->selectChunkToUpload();
    if (chunk == -1)
    {
        handleAllUploaded();
        return;
    }

    if (!d->connection())
        return;

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

    NX_VERBOSE(this, "Uploading chunk %1 for %2", chunk, d->upload.destination);

    const QByteArray& data = d->readChunk(chunk);

    const auto callback = nx::utils::guarded(this,
        [this, chunk](
            bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
        {
            {
                NX_MUTEX_LOCKER lock(&d->mutex);
                d->requests.releaseHandle(handle);
            }
            handleChunkUploaded(success, chunk);
        });

    d->requests.storeHandle(d->connectedServerApi()->uploadFileChunk(
        d->server->getId(),
        d->upload.destination,
        chunk,
        data,
        callback,
        thread()));
}

void UploadWorker::handleChunkUploaded(bool success, int chunkIndex)
{
    NX_VERBOSE(this, "Chunk %1 was uploaded for %2: %3",
        chunkIndex, d->upload.destination, success);

    if (!success)
    {
        handleError(tr("Could not upload file chunk to the server"));
        return;
    }

    d->uploadedChunks[chunkIndex] = true;
    d->upload.uploaded = std::min<qint64>(
        d->uploadedChunks.count(true) * d->chunkSize, d->upload.size);

    handleUpload();
}

void UploadWorker::handleAllUploaded()
{
    NX_INFO(this, "Upload for %1 is done. Checking...", d->upload.destination);

    if (!d->connection())
        return;

    d->upload.status = UploadState::Checking;

    emitProgress();

    const auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            {
                NX_MUTEX_LOCKER lock(&d->mutex);
                d->requests.releaseHandle(handle);
            }

            FileInformation info = result.deserialized<FileInformation>();
            const bool fileStatusOk = info.status == FileInformation::Status::downloaded;
            handleCheckFinished(success, fileStatusOk);
        });

    d->requests.storeHandle(d->connectedServerApi()->fileDownloadStatus(
        d->server->getId(),
        d->upload.destination,
        callback,
        thread()));
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
