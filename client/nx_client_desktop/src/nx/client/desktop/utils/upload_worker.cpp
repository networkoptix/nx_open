#include "upload_worker.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

class QnClientUploadWorkerPrivate
{
public:
    QnFileUpload upload;
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

QnClientUploadWorker::QnClientUploadWorker(const QnMediaServerResourcePtr& server, const QString& path, qint64 ttl, QObject* parent):
    QObject(parent),
    d(new QnClientUploadWorkerPrivate)
{
    d->server = server;
    d->file.reset(new QFile(path));
    d->ttl = ttl;

    connect(&d->md5FutureWatcher, &QFutureWatcherBase::finished,
        this, &QnClientUploadWorker::handleMd5Calculated);
}

QnClientUploadWorker::~QnClientUploadWorker()
{
    handleStop();
}

QnFileUpload QnClientUploadWorker::start()
{
    NX_ASSERT(d->upload.status == QnFileUpload::Initial);

    d->upload.id = lit("tmp-") + QnUuid::createUuid().toSimpleString();
    QFileInfo info(d->file->fileName());
    if (!info.suffix().isEmpty())
        d->upload.id += lit(".") + info.suffix();

    if (!d->file->open(QIODevice::ReadOnly))
    {
        d->upload.status = QnFileUpload::Error;
        d->upload.errorMessage = tr("Could not open file \"%1\"").arg(d->file->fileName());
        return d->upload;
    }

    d->upload.size = d->file->size();
    d->totalChunks = (d->upload.size + d->chunkSize - 1) / d->chunkSize;

    d->upload.status = QnFileUpload::CalculatingMD5;

    emitProgress();

    QSharedPointer<QFile> fileCopy = d->file;
    d->md5Future = QtConcurrent::run(
        [fileCopy]() {
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

void QnClientUploadWorker::cancel()
{
    QnFileUpload::Status status = d->upload.status;
    if (status == QnFileUpload::Initial || status == QnFileUpload::Done ||
        status == QnFileUpload::Error || status == QnFileUpload::Canceled)
        return;

    handleStop();
    d->upload.status = QnFileUpload::Canceled;
    /* We don't emit signals here as canceling is also a way of silencing the worker. */
}

QnFileUpload QnClientUploadWorker::status() const
{
    return d->upload;
}

void QnClientUploadWorker::emitProgress()
{
    NX_ASSERT(d->upload.status != QnFileUpload::Canceled);
    NX_ASSERT(d->upload.status != QnFileUpload::Initial);

    emit progress(d->upload);
}

void QnClientUploadWorker::handleStop()
{
    d->md5FutureWatcher.disconnect(this);

    if (d->runningHandle)
        d->server->restConnection()->cancelRequest(d->runningHandle);
    d->runningHandle = 0;

    QnFileUpload::Status status = d->upload.status;
    if (status == QnFileUpload::CreatingUpload || status == QnFileUpload::Uploading ||
        status == QnFileUpload::Checking)
        d->server->restConnection()->removeFileDownload(d->upload.id, true, nullptr);
}

void QnClientUploadWorker::handleError(const QString& message)
{
    handleStop();

    d->upload.status = QnFileUpload::Error;
    d->upload.errorMessage = message;
    emitProgress();
}

void QnClientUploadWorker::handleMd5Calculated()
{
    QByteArray md5 = d->md5Future.result();
    if (md5.isEmpty())
    {
        handleError(tr("Could not calculate md5 for file \"%1\"").arg(d->file->fileName()));
        return;
    }

    d->md5 = md5;
    d->upload.status = QnFileUpload::CreatingUpload;

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

void QnClientUploadWorker::handleFileUploadCreated(bool success, rest::Handle handle)
{
    NX_ASSERT(d->runningHandle == handle);
    d->runningHandle = 0;

    if (!success)
    {
        handleError(tr("Could not create upload on the server side"));
        return;
    }

    d->upload.status = QnFileUpload::Uploading;
    handleUpload();
}

void QnClientUploadWorker::handleUpload()
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

void QnClientUploadWorker::handleChunkUploaded(bool success, rest::Handle handle)
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

void QnClientUploadWorker::handleAllUploaded()
{
    d->upload.status = QnFileUpload::Checking;

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

void QnClientUploadWorker::handleCheckFinished(bool success, rest::Handle handle, bool ok)
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

    d->upload.status = QnFileUpload::Done;

    emitProgress();
}

