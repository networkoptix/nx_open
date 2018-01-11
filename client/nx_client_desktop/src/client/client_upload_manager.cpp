#include "client_upload_manager.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/vms/common/p2p/downloader/file_information.h>

class QnFileUploadData: public QObject
{
public:
    QnFileUploadData(const QnMediaServerResourcePtr& server, const QString& path, QObject* parent = nullptr):
        QObject(parent),
        server(server),
        file(path)
    {
        upload.id = lit("tmp-") + QnUuid::createUuid().toSimpleString();
        QFileInfo info(path);
        if (!info.suffix().isEmpty())
            upload.id += lit(".") + info.suffix();

        if (file.open(QIODevice::ReadOnly))
        {
            upload.status = QnFileUpload::Opened;
            upload.size = file.size();
            totalChunks = (upload.size + chunkSize - 1) / chunkSize;
        }
        else
        {
            upload.status = QnFileUpload::Error;
            upload.errorMessage = QnClientUploadManager::tr("Could not open file \"%1\"").arg(path);
        }
    }

    QnFileUpload upload;
    QnMediaServerResourcePtr server;
    QFile file;
    QByteArray md5;
    int chunkSize = 1024 * 1024;
    int currentChunk = 0;
    int totalChunks = 0;

    QFuture<QByteArray> md5Future;
    QFutureWatcher<QByteArray> md5FutureWatcher;
    rest::Handle runningHandle = 0;
};

QnClientUploadManager::QnClientUploadManager(QObject* parent):
    QObject(parent)
{
}

QnClientUploadManager::~QnClientUploadManager()
{
    if (m_dataList.empty())
        return;

    QnFileUploadData* data = m_dataList.front();
    data->md5Future.waitForFinished();
    data->server->restConnection()->cancelRequest(data->runningHandle);
}

QnFileUpload QnClientUploadManager::addUpload(const QnMediaServerResourcePtr& server, const QString& path)
{
    QnFileUploadData* data = new QnFileUploadData(server, path, this);

    if (data->upload.status == QnFileUpload::Error)
    {
        data->deleteLater();
        return data->upload;
    }

    m_dataList.push_back(data);

    advance();
    return data->upload;
}

QnFileUploadData* QnClientUploadManager::currentData() const
{
    NX_ASSERT(!m_dataList.isEmpty());

    return m_dataList.front();
}

QnFileUploadData* QnClientUploadManager::dropCurrentData()
{
    QnFileUploadData* data = currentData();
    m_dataList.pop_front();
    data->deleteLater();
    return data;
}

void QnClientUploadManager::handleError(const QString& message)
{
    QnFileUploadData* data = dropCurrentData();

    if (data->upload.status >= QnFileUpload::CreatingUpload)
        data->server->restConnection()->removeFileDownload(data->upload.id, true, nullptr);

    data->upload.status = QnFileUpload::Error;
    data->upload.errorMessage = message;
    emit progress(data->upload);

    advance();
}

void QnClientUploadManager::handleStarted()
{
    QnFileUploadData* data = currentData();

    data->upload.status = QnFileUpload::CalculatingMD5;

    emit progress(data->upload);

    connect(&data->md5FutureWatcher, &QFutureWatcherBase::finished, this, &QnClientUploadManager::handleMd5Calculated);
    data->md5Future = QtConcurrent::run(
        [this, data]() {
            nx::utils::QnCryptographicHash hash(nx::utils::QnCryptographicHash::Md5);
            if (hash.addData(&data->file))
                return hash.result();
            else
                return QByteArray();
        }
    );
    data->md5FutureWatcher.setFuture(data->md5Future);
}

void QnClientUploadManager::handleMd5Calculated()
{
    QnFileUploadData* data = currentData();

    QByteArray md5 = data->md5Future.result();
    if (md5.isEmpty())
    {
        handleError(tr("Could not calculate md5 for file \"%1\"").arg(data->file.fileName()));
        return;
    }

    data->md5 = md5;
    data->upload.status = QnFileUpload::CreatingUpload;

    emit progress(data->upload);

    auto callback = [this](bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
    {
        this->handleFileUploadCreated(success, handle);
    };

    data->runningHandle = data->server->restConnection()->addFileUpload(
        data->upload.id,
        data->upload.size,
        data->chunkSize,
        data->md5.toHex(),
        callback,
        thread());
}

void QnClientUploadManager::handleFileUploadCreated(bool success, rest::Handle handle)
{
    QnFileUploadData* data = currentData();

    NX_ASSERT(data->runningHandle == handle);
    data->runningHandle = 0;

    if (!success)
    {
        handleError(tr("Could not create upload on the server side"));
        return;
    }

    data->upload.status = QnFileUpload::Uploading;
    handleUpload();
}

void QnClientUploadManager::handleUpload()
{
    QnFileUploadData* data = currentData();

    if (data->currentChunk == data->totalChunks) {
        handleAllUploaded();
        return;
    }

    emit progress(data->upload);

    bool seekOk = data->file.seek(static_cast<qint64>(data->chunkSize) * data->currentChunk);
    QByteArray bytes = data->file.read(data->chunkSize);
    if (!seekOk || bytes.isEmpty())
    {
        handleError(data->file.errorString());
        return;
    }

    auto callback = [this](bool success, rest::Handle handle, const rest::ServerConnection::EmptyResponseType&)
    {
        this->handleChunkUploaded(success, handle);
    };

    data->runningHandle = data->server->restConnection()->uploadFileChunk(
        data->upload.id,
        data->currentChunk,
        bytes,
        callback,
        thread());
    data->currentChunk++;
}

void QnClientUploadManager::handleChunkUploaded(bool success, rest::Handle handle)
{
    QnFileUploadData* data = currentData();

    NX_ASSERT(data->runningHandle == handle);
    data->runningHandle = 0;

    if (!success)
    {
        handleError(tr("Could not upload file chunk to the server"));
        return;
    }

    data->upload.uploaded = qMin(data->upload.uploaded + data->chunkSize, data->upload.size);
    handleUpload();
}

void QnClientUploadManager::handleAllUploaded()
{
    QnFileUploadData* data = currentData();

    data->upload.status = QnFileUpload::Checking;

    emit progress(data->upload);

    auto callback = [this](bool success, rest::Handle handle, const QnJsonRestResult& result)
    {
        using namespace nx::vms::common::p2p::downloader;
        FileInformation info = result.deserialized<FileInformation>();
        this->handleCheckFinished(success, handle, info.status == FileInformation::Status::downloaded);
    };

    data->runningHandle = data->server->restConnection()->fileDownloadStatus(
        data->upload.id,
        callback,
        thread());
}

void QnClientUploadManager::handleCheckFinished(bool success, rest::Handle handle, bool ok)
{
    QnFileUploadData* data = currentData();

    NX_ASSERT(data->runningHandle == handle);
    data->runningHandle = 0;

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

    data->upload.status = QnFileUpload::Done;

    emit progress(data->upload);

    dropCurrentData();
    advance();
}

void QnClientUploadManager::advance()
{
    if (m_dataList.empty())
        return;

    QnFileUploadData* data = currentData();
    NX_ASSERT(data->upload.status == QnFileUpload::Opened);
    handleStarted();
}
