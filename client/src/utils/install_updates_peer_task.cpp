#include "install_updates_peer_task.h"

#include <QtCore/QTimer>

#include <api/app_server_connection.h>
#include <nx_ec/ec_api.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace {
    const int checkTimeout = 10 * 60 * 1000;

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }
}

QnInstallUpdatesPeerTask::QnInstallUpdatesPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent)
{
    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(checkTimeout);
    m_checkTimer->setSingleShot(true);

    connect(m_checkTimer,   &QTimer::timeout,                       this,           &QnInstallUpdatesPeerTask::at_checkTimeout);
    connect(this,           &QnInstallUpdatesPeerTask::finished,    m_checkTimer,   &QTimer::stop);
}

void QnInstallUpdatesPeerTask::setUpdateId(const QString &updateId) {
    m_updateId = updateId;
}

void QnInstallUpdatesPeerTask::setVersion(const QnSoftwareVersion &version) {
    m_version = version;
}

void QnInstallUpdatesPeerTask::doStart() {
    if (peers().isEmpty()) {
        finish(NoError);
        return;
    }

    connect(qnResPool, &QnResourcePool::resourceChanged, this, &QnInstallUpdatesPeerTask::at_resourceChanged);

    m_restartingPeers = m_pendingPeers = peers();

    connection2()->getUpdatesManager()->installUpdate(m_updateId, m_pendingPeers,
                                                      this, [this](int, ec2::ErrorCode){});
    m_checkTimer->start();
}

void QnInstallUpdatesPeerTask::at_resourceChanged(const QnResourcePtr &resource) {
    QnId peerId = resource->getId();

    if (!peers().contains(peerId))
        return;

    if (resource->getStatus() == QnResource::Offline)
        m_restartingPeers.remove(peerId);

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    if (server->getVersion() == m_version) {
        m_pendingPeers.remove(peerId);
        emit peerFinished(peerId);
    }

    if (m_pendingPeers.isEmpty()) {
        disconnect(qnResPool, &QnResourcePool::resourceChanged, this, &QnInstallUpdatesPeerTask::at_resourceChanged);
        finish(NoError);
        return;
    }

    if (m_restartingPeers.isEmpty()) {
        bool pending = false;
        foreach (const QnId &id, m_pendingPeers) {
            QnResourcePtr resource = qnResPool->getIncompatibleResourceById(id, true);
            if (resource->getStatus() == QnResource::Offline) {
                pending = true;
                break;
            }
        }

        if (!pending)
            finish(InstallationFailed);
    }
}

void QnInstallUpdatesPeerTask::at_checkTimeout() {
    /* disconnect from resource pool and make final check */
    disconnect(qnResPool, &QnResourcePool::resourceChanged, this, &QnInstallUpdatesPeerTask::at_resourceChanged);

    foreach (const QnId &id, m_pendingPeers) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (server->getVersion() == m_version) {
            m_pendingPeers.remove(id);
            emit peerFinished(id);
        }
    }

    finish(m_pendingPeers.isEmpty() ? NoError : InstallationFailed);
}
