#include "upload_manager.h"

#include <memory>

namespace nx {
namespace client {
namespace desktop {

UploadManager::UploadManager(QObject* parent):
    QObject(parent)
{
}

UploadManager::~UploadManager()
{
}

FileUpload UploadManager::addUpload(
    const QnMediaServerResourcePtr& server,
    const QString& path,
    qint64 ttl,
    QObject* context,
    Callback callback)
{
    std::unique_ptr<UploadWorker> worker = std::make_unique<UploadWorker>(server, path, ttl, this);

    FileUpload result = worker->start();
    if (result.status == FileUpload::Error)
        return result;

    if (callback)
        connect(worker.get(), &UploadWorker::progress, context, callback);

    connect(worker.get(), &UploadWorker::progress, this,
        [this](const FileUpload& upload)
        {
            FileUpload::Status status = upload.status;
            if (status == FileUpload::Error || status == FileUpload::Done)
            {
                m_workers[upload.id]->deleteLater();
                m_workers.remove(upload.id);
            }
        }
    );

    m_workers[result.id] = worker.release();
    return result;
}

void UploadManager::cancelUpload(const QString& id)
{
    auto pos = m_workers.find(id);
    if (pos == m_workers.end())
        return;

    pos.value()->cancel();
    pos.value()->deleteLater();
    m_workers.erase(pos);
}

} // namespace desktop
} // namespace client
} // namespace nx
