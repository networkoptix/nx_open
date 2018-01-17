#include "upload_worker.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

namespace nx {
namespace client {
namespace desktop {

class UploadWorkerPrivate
{
public:
    FileUpload upload;
    QnMediaServerResourcePtr server;
    QSharedPointer<QFile> file;
    qint64 ttl = 0;
    QByteArray md5;
    int chunkSize = 1024 * 1024;
    int currentChunk = 0;
    int totalChunks = 0;

    QFuture<QByteArray> md5Future;
    QFutureWatcher<QByteArray> md5FutureWatcher;
    rest::Handle runningHandle = 0;
};

UploadWorker::UploadWorker(const QnMediaServerResourcePtr& server, const QString& path, qint64 ttl, QObject* parent):
    QObject(parent),
    d(new UploadWorkerPrivate)
{
    d->server = server;
    d->file.reset(new QFile(path));
    d->ttl = ttl;

    connect(&d->md5FutureWatcher, &QFutureWatcherBase::finished,
        this, &UploadWorker::handleMd5Calculated);
}

UploadWorker::~UploadWorker()
{
    handleStop();
}

FileUpload UploadWorker::start()
{
    NX_ASSERT(d->upload.status == FileUpload::Initial);

    d->upload.id = lit("tmp-") + QnUuid::createUuid().toSimpleString();
    QFileInfo info(d->file->fileName());
    if (!info.suffix().isEmpty())
        d->upload.id += lit(".") + info.suffix();

    if (!d->file->open(QIODevice::ReadOnly))
    {
        d->upload.status = FileUpload::Error;
        d->upload.errorMessage = tr("Could not open file \"%1\"").arg(d->file->fileName());
        return d->upload;
    }

    d->upload.size = d->file->size();
    d->totalChunks = (d->upload.size + d->chunkSize - 1) / d->chunkSize;

    d->upload.status = FileUpload::CalculatingMD5;

    emitProgress();

    QSharedPointer<QFile> fileCopy = d->file;
    d->md5Future = QtConcurrent::run(
        [fileCopy]()
        {
            nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Md5);
            if (hash.addData(fileCopy.data()))
                return hash.result();
            else
                return QByteArray();
        }
    );
    d->md5FutureWatcher.setFuture(d->md5Future);

    return d->upload;
}

void UploadWorker::cancel()
{
    FileUpload::Status status = d->upload.status;
    if (status == FileUpload::Initial || status == FileUpload::Done ||
        status == FileUpload::Error || status == FileUpload::Canceled)
    {
        return;
    }

    handleStop();
    d->upload.status = FileUpload::Canceled;
    /* We don't emit signals here as canceling is also a way of silencing the worker. */
}

FileUpload UploadWorker::status() const
{
    return d->upload;
}

void UploadWorker::emitProgress()
{
    NX_ASSERT(d->upload.status != FileUpload::Canceled);
    NX_ASSERT(d->upload.status != FileUpload::Initial);

    emit progress(d->upload);
}

void UploadWorker::handleStop()
{
    d->md5FutureWatcher.disconnect(this);

    if (d->runningHandle)
        d->server->restConnection()->cancelRequest(d->runningHandle);
    d->runningHandle = 0;

    FileUpload::Status status = d->upload.status;
    if (status == FileUpload::CreatingUpload || status == FileUpload::Uploading ||
        status == FileUpload::Checking)
        d->server->restConnection()->removeFileDownload(d->upload.id, true, nullptr);
}

void UploadWorker::handleError(const QString& message)
{
    handleStop();

    d->upload.status = FileUpload::Error;
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
    d->upload.status = FileUpload::CreatingUpload;

    emitProgress();

    auto callback = [this](bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
    {
        this->handleFileUploadCreated(success, handle);
    };

    d->runningHandle = d->server->restConnection()->addFileUpload(
        d->upload.id,
        d->upload.size,
        d->chunkSize,
        d->md5.toHex(),
        d->ttl,
        callback,
        thread());
}

void UploadWorker::handleFileUploadCreated(bool success, rest::Handle handle)
{
    NX_ASSERT(d->runningHandle == handle);
    d->runningHandle = 0;

    if (!success)
    {
        handleError(tr("Could not create upload on the server side"));
        return;
    }

    d->upload.status = FileUpload::Uploading;
    handleUpload();
}

void UploadWorker::handleUpload()
{
    if (d->currentChunk == d->totalChunks) {
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

    auto callback = [this](bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
    {
        this->handleChunkUploaded(success, handle);
    };

    d->runningHandle = d->server->restConnection()->uploadFileChunk(
        d->upload.id,
        d->currentChunk,
        bytes,
        callback,
        thread());
    d->currentChunk++;
}

void UploadWorker::handleChunkUploaded(bool success, rest::Handle handle)
{
    NX_ASSERT(d->runningHandle == handle);
    d->runningHandle = 0;

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
    d->upload.status = FileUpload::Checking;

    emitProgress();

    auto callback = [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
    {
        using namespace nx::vms::common::p2p::downloader;
        FileInformation info = result.deserialized<FileInformation>();
        this->handleCheckFinished(success, handle, info.status == FileInformation::Status::downloaded);
    };

    d->runningHandle = d->server->restConnection()->fileDownloadStatus(
        d->upload.id,
        callback,
        thread());
}

void UploadWorker::handleCheckFinished(bool success, rest::Handle handle, bool ok)
{
    NX_ASSERT(d->runningHandle == handle);
    d->runningHandle = 0;

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

    d->upload.status = FileUpload::Done;

    emitProgress();
}

} // namespace desktop
} // namespace client
} // namespace nx
