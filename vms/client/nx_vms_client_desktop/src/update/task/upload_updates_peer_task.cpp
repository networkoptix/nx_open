#include "upload_updates_peer_task.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <update/update_uploader.h>

#include <nx/utils/log/log.h>

QnUploadUpdatesPeerTask::QnUploadUpdatesPeerTask(QObject* parent):
    QnNetworkPeerTask(parent),
    m_uploader(new QnUpdateUploader(this))
{
    connect(m_uploader, &QnUpdateUploader::finished,
        this, &QnUploadUpdatesPeerTask::at_uploader_finished);
    connect(m_uploader, &QnUpdateUploader::progressChanged,
        this, &QnUploadUpdatesPeerTask::at_uploader_progressChanged);
    connect(m_uploader, &QnUpdateUploader::peerProgressChanged,
        this, &QnUploadUpdatesPeerTask::peerProgressChanged);
}

void QnUploadUpdatesPeerTask::setUploads(
    const QHash<nx::vms::api::SystemInformation, QString>& fileBySystemInformation)
{
    m_fileBySystemInformation = fileBySystemInformation;
}

void QnUploadUpdatesPeerTask::setUpdateId(const QString& updateId)
{
    m_updateId = updateId;
}

void QnUploadUpdatesPeerTask::doCancel()
{
    m_pendingUploads.clear();
    if (m_uploader)
        m_uploader->cancel();
}

void QnUploadUpdatesPeerTask::doStart()
{
    m_peersBySystemInformation.clear();
    m_finishedPeers.clear();

    NX_DEBUG(this, lit("Update: Starting upload."));

    for (const auto& id: peers())
    {
        const auto server = resourcePool()->getIncompatibleServerById(id, true);
        if (!server)
            continue;

        if (!m_fileBySystemInformation.contains(server->getSystemInfo()))
        {
            finish(ConsistencyError);
            return;
        }

        m_peersBySystemInformation.insert(server->getSystemInfo(), id);
    }

    m_pendingUploads = m_peersBySystemInformation.uniqueKeys();

    NX_DEBUG(this, lit("Update: Have to upload %1 files.").arg(m_pendingUploads.size()));

    uploadNextUpdate();
}

void QnUploadUpdatesPeerTask::uploadNextUpdate()
{
    if (m_pendingUploads.isEmpty())
    {
        NX_DEBUG(this, lit("Update: Upload finished."));
        finish(NoError);
        return;
    }

    const auto systemInformation = m_pendingUploads.first();

    NX_DEBUG(this, lit("Update: Starting upload [%1 : %2].").arg(
        systemInformation.toString(), m_fileBySystemInformation[systemInformation]));

    if (!m_uploader->uploadUpdate(m_updateId, m_fileBySystemInformation[systemInformation],
        m_peersBySystemInformation.values(systemInformation).toSet()))
    {
        NX_DEBUG(this, lit("Update: Uploader failure."));
        finish(UploadError);
        return;
    }
}

void QnUploadUpdatesPeerTask::at_uploader_finished(int errorCode, const QSet<QnUuid>& failedPeers)
{
    switch (errorCode)
    {
        case QnUpdateUploader::NoError:
        {
            for (const auto& id: m_uploader->peers())
            {
                emit peerFinished(id);
                m_finishedPeers.insert(id);
            }

            NX_DEBUG(this, lit("Update: Finished upload [%1].")
                .arg(m_pendingUploads.first().toString()));

            m_pendingUploads.removeFirst();
            uploadNextUpdate();

            break;
        }

        case QnUpdateUploader::NoFreeSpace:
            NX_DEBUG(this, lit("Update: No free space."));
            finish(NoFreeSpaceError, failedPeers);
            break;

        case QnUpdateUploader::TimeoutError:
            NX_DEBUG(this, lit("Update: Timeout."));
            finish(TimeoutError, failedPeers);
            break;

        case QnUpdateUploader::AuthenticationError:
            NX_DEBUG(this, lit("Update: Authentication failed."));
            finish(AuthenticationError, failedPeers);
            break;

        default:
            NX_DEBUG(this, lit("Update: Unknown error."));
            finish(UploadError, failedPeers);
            break;
    }
}

void QnUploadUpdatesPeerTask::at_uploader_progressChanged(int progress)
{
    const int peers = m_peersBySystemInformation.size();
    const int finished = m_finishedPeers.size();
    const int current = m_uploader->peers().size();

    emit progressChanged((finished * 100 + current * progress) / peers);
}
