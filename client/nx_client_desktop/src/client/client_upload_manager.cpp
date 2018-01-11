#include "client_upload_manager.h"

#include <memory>

QnClientUploadManager::QnClientUploadManager(QObject* parent):
    QObject(parent)
{
}

QnClientUploadManager::~QnClientUploadManager()
{
}

QnFileUpload QnClientUploadManager::addUpload(const QnMediaServerResourcePtr& server, const QString& path)
{
    std::unique_ptr<QnClientUploadWorker> worker = std::make_unique<QnClientUploadWorker>(server, path, this);

    QnFileUpload result = worker->start();
    if (result.status == QnFileUpload::Error)
        return result;

    connect(worker.get(), &QnClientUploadWorker::progress, this,
        [this](const QnFileUpload& upload)
        {
            emit progress(upload);

            QnFileUpload::Status status = upload.status;
            if (status == QnFileUpload::Error || status == QnFileUpload::Done || status == QnFileUpload::Canceled)
            {
                m_workers[upload.id]->deleteLater();
                m_workers.remove(upload.id);
            }
        }
    );

    m_workers[result.id] = worker.release();
    return result;
}

