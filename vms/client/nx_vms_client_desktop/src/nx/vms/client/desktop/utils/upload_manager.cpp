#include "upload_manager.h"

#include <nx/utils/std/cpp14.h>

#include "upload_worker.h"

namespace nx::vms::client::desktop {

UploadManager::UploadManager(QObject* parent):
    QObject(parent)
{
}

UploadManager::~UploadManager()
{
    qDeleteAll(m_workers);
    m_workers.clear();
}

QString UploadManager::addUpload(
        const QnMediaServerResourcePtr& server,
        UploadState& config,
        QObject* context,
        Callback callback)
{
    std::unique_ptr<UploadWorker> worker = std::make_unique<UploadWorker>(server, config, this);

    worker->start();
    UploadState state = worker->state();
    if (state.status == UploadState::Error)
    {
        config.errorMessage = state.errorMessage;
        return QString();
    }

    if (callback)
        connect(worker.get(), &UploadWorker::progress, context, callback);

    connect(worker.get(), &UploadWorker::progress, this,
        [this](const UploadState& upload)
        {
            UploadState::Status status = upload.status;
            if (status == UploadState::Error || status == UploadState::Done)
            {
                m_workers[upload.id]->deleteLater();
                m_workers.remove(upload.id);
            }
        }
    );

    m_workers[state.id] = worker.release();
    return state.id;
}

UploadState UploadManager::state(const QString& id)
{
    auto pos = m_workers.find(id);
    if (pos == m_workers.end())
        return UploadState();

    return pos.value()->state();
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

} // namespace nx::vms::client::desktop
