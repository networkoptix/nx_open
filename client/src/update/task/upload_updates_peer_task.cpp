#include "upload_updates_peer_task.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <update/update_uploader.h>
#include <utils/common/log.h>

QnUploadUpdatesPeerTask::QnUploadUpdatesPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_uploader(new QnUpdateUploader(this))
{
    connect(m_uploader,     &QnUpdateUploader::finished,            this,   &QnUploadUpdatesPeerTask::at_uploader_finished);
    connect(m_uploader,     &QnUpdateUploader::progressChanged,     this,   &QnUploadUpdatesPeerTask::at_uploader_progressChanged);
    connect(m_uploader,     &QnUpdateUploader::peerProgressChanged, this,   &QnUploadUpdatesPeerTask::peerProgressChanged);
}

void QnUploadUpdatesPeerTask::setUploads(const QHash<QnSystemInformation, QString> &fileBySystemInformation) {
    m_fileBySystemInformation = fileBySystemInformation;
}

void QnUploadUpdatesPeerTask::setUpdateId(const QString &updateId) {
    m_updateId = updateId;
}

void QnUploadUpdatesPeerTask::doCancel() {
    m_pendingUploads.clear();
    if (m_uploader)
        m_uploader->cancel();
}

void QnUploadUpdatesPeerTask::doStart() {
    m_peersBySystemInformation.clear();
    m_finishedPeers.clear();

    NX_LOG(lit("Update: QnUploadUpdatesPeerTask: Starting upload."), cl_logDEBUG1);

    foreach (const QnUuid &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (!m_fileBySystemInformation.contains(server->getSystemInfo())) {
            finish(ConsistencyError);
            return;
        }

        m_peersBySystemInformation.insert(server->getSystemInfo(), id);
    }

    m_pendingUploads = m_peersBySystemInformation.uniqueKeys();

    NX_LOG(lit("Update: QnUploadUpdatesPeerTask: Have to upload %1 file(s).").arg(m_pendingUploads.size()), cl_logDEBUG1);

    uploadNextUpdate();
}

void QnUploadUpdatesPeerTask::uploadNextUpdate() {
    if (m_pendingUploads.isEmpty()) {
        NX_LOG(lit("Update: QnUploadUpdatesPeerTask: Upload finished."), cl_logDEBUG1);
        finish(NoError);
        return;
    }

    QnSystemInformation systemInformation = m_pendingUploads.first();

    NX_LOG(lit("Update: QnUploadUpdatesPeerTask: Starting upload [%1 : %2].")
           .arg(systemInformation.toString()).arg(m_fileBySystemInformation[systemInformation]), cl_logDEBUG1);

    if (!m_uploader->uploadUpdate(m_updateId,
                                  m_fileBySystemInformation[systemInformation],
                                  QSet<QnUuid>::fromList(m_peersBySystemInformation.values(systemInformation))))
    {
        NX_LOG(lit("Update: QnUploadUpdatesPeerTask: Uploader failure."), cl_logDEBUG1);
        finish(UploadError);
        return;
    }
}

void QnUploadUpdatesPeerTask::at_uploader_finished(int errorCode, const QSet<QnUuid> &failedPeers) {
    switch (errorCode) {
    case QnUpdateUploader::NoError:
        foreach (const QnUuid &id, m_uploader->peers()) {
            emit peerFinished(id);
            m_finishedPeers.insert(id);
        }

        NX_LOG(lit("Update: QnUploadUpdatesPeerTask: Finished upload [%1].").arg(m_pendingUploads.first().toString()), cl_logDEBUG1);

        m_pendingUploads.removeFirst();
        uploadNextUpdate();

        break;
    case QnUpdateUploader::NoFreeSpace:
        NX_LOG(lit("Update: QnUploadUpdatesPeerTask: No free space."), cl_logDEBUG1);
        finish(NoFreeSpaceError, failedPeers);
        break;
    case QnUpdateUploader::TimeoutError:
        NX_LOG(lit("Update: QnUploadUpdatesPeerTask: Timeout."), cl_logDEBUG1);
        finish(TimeoutError, failedPeers);
        break;
    default:
        NX_LOG(lit("Update: QnUploadUpdatesPeerTask: Unknown error."), cl_logDEBUG1);
        finish(UploadError, failedPeers);
        break;
    }
}

void QnUploadUpdatesPeerTask::at_uploader_progressChanged(int progress) {
    int peers = m_peersBySystemInformation.size();
    int finished = m_finishedPeers.size();
    int current = m_uploader->peers().size();

    emit progressChanged((finished * 100 + current * progress) / peers);
}
