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
    Private(const QnMediaServerResourcePtr& server) :
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
    int totalChunks = 0;
    QnMutex mutex;

    QFuture<QByteArray> md5Future;
    QFutureWatcher<QByteArray> md5FutureWatcher;
    ServerRequestStorage requests;
};

UploadWorker::UploadWorker(const QnMediaServerResourcePtr& server, UploadState& config, QObject* parent):
    QObject(parent),
    d(new Private(server))
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

    d->upload.id = lit("tmp-") + QnUuid::createUuid().toSimpleString();
    QFileInfo info(d->file->fileName());
    if (!info.suffix().isEmpty())
        d->upload.id += lit(".") + info.suffix();

    if (d->upload.destination.isEmpty())
        d->upload.destination = d->upload.id;

    if (!d->file->open(QIODevice::ReadOnly))
    {
        d->upload.status = UploadState::Error;
        d->upload.errorMessage = tr("Could not open file \"%1\"").arg(d->file->fileName());
        return;
    }

    d->upload.size = d->file->size();
    d->totalChunks = (d->upload.size + d->chunkSize - 1) / d->chunkSize;

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
        callback, thread());
    // We have a race condition around this handle.
    d->requests.storeHandle(handle);
}

void UploadWorker::handleFileUploadCreated(bool success, RemoteResult code, QString error)
{
    if (!success)
    {
        switch (code)
        {
            case RemoteResult::ok:
                // Should not be here. success should be true
                NX_ASSERT(false);
                d->upload.status = UploadState::Uploading;
                handleUpload();
                break;
            case RemoteResult::fileAlreadyExists:
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
    if (d->currentChunk == d->totalChunks)
    {
        handleAllUploaded();
        return;
    }

    emitProgress();

    bool seekOk = d->file->seek(static_cast<qint64>(d->chunkSize) * d->currentChunk);
    QByteArray bytes = d->file->read(d->chunkSize);
    if (!seekOk || bytes.isEmpty())
    {
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
    d->currentChunk++;
}

void UploadWorker::handleChunkUploaded(bool success)
{
    if (!success)
    {
        handleError(tr("Could not upload file chunk to the server"));
        return;
    }

    d->upload.uploaded = qMin(d->upload.uploaded + d->chunkSize, d->upload.size);
    handleUpload();
}

void UploadWorker::handleAllUploaded()
{
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
