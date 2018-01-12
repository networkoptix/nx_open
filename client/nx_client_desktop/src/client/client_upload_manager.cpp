#include "client_upload_manager.h"

#include <memory>

QnClientUploadManager::QnClientUploadManager(QObject* parent):
    QObject(parent)
{
}

QnClientUploadManager::~QnClientUploadManager()
{
}

QnFileUpload QnClientUploadManager::addUpload(
    const QnMediaServerResourcePtr& server,
    const QString& path,
    QObject* context,
    Callback callback)
{
    std::unique_ptr<QnClientUploadWorker> worker = std::make_unique<QnClientUploadWorker>(server, path, this);

    QnFileUpload result = worker->start();
    if (result.status == QnFileUpload::Error)
        return result;

    if (callback)
        connect(worker.get(), &QnClientUploadWorker::progress, context, callback);

    connect(worker.get(), &QnClientUploadWorker::progress, this,
        [this](const QnFileUpload& upload)
        {
            QnFileUpload::Status status = upload.status;
            if (status == QnFileUpload::Error || status == QnFileUpload::Done)
            {
                m_workers[upload.id]->deleteLater();
                m_workers.remove(upload.id);
            }
        }
    );

    m_workers[result.id] = worker.release();
    return result;
}

void QnClientUploadManager::cancelUpload(const QString& id)
{
    auto pos = m_workers.find(id);
    if (pos == m_workers.end())
        return;

    pos.value()->cancel();
    pos.value()->deleteLater();
    m_workers.erase(pos);
}

