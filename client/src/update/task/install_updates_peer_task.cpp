#include "install_updates_peer_task.h"

#include <QtCore/QTimer>

#include <api/app_server_connection.h>
#include <nx_ec/ec_api.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <client/client_message_processor.h>

namespace {
    const int checkTimeout = 15 * 60 * 1000;
    const int shortTimeout = 60 * 1000;

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }
}

QnInstallUpdatesPeerTask::QnInstallUpdatesPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent)
{
    m_checkTimer = new QTimer(this);
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

    m_stoppingPeers = m_restartingPeers = m_pendingPeers = peers();

    static_cast<QnClientMessageProcessor*>(QnClientMessageProcessor::instance())->setHoldConnection(true);

    connection2()->getUpdatesManager()->installUpdate(m_updateId, m_pendingPeers,
                                                      this, [this](int, ec2::ErrorCode){});
    m_checkTimer->start(checkTimeout);

    int peersSize = m_pendingPeers.size();

    QTimer *progressTimer = new QTimer(this);
    progressTimer->setInterval(1000);
    progressTimer->setSingleShot(false);
    connect(progressTimer, &QTimer::timeout,  this, [this, peersSize] {
        int progress = (checkTimeout - m_checkTimer->remainingTime()) * 100 / checkTimeout;
        progress = qBound(0, progress, 100);
        
        /* Count finished peers. */
        int totalProgress = (peersSize - m_pendingPeers.size()) * 100;

        foreach (const QnUuid &peerId, m_pendingPeers) {
            int peerProgress = progress;
            /* Peer already restarted. */
            if (!m_restartingPeers.contains(peerId))
                peerProgress = qMax(progress, 99);
            /* Peer already stopped. */
            else if (!m_stoppingPeers.contains(peerId))
                peerProgress = qMax(progress, 95);
            emit peerProgressChanged(peerId, peerProgress);

            /* Count processing peers. */
            totalProgress += peerProgress;
        }      

        emit progressChanged(totalProgress / peersSize);
    });
    connect(this,   &QnInstallUpdatesPeerTask::finished,    progressTimer,    &QTimer::stop);
    progressTimer->start();
}

void QnInstallUpdatesPeerTask::at_resourceChanged(const QnResourcePtr &resource) {
    QnUuid peerId = resource->getId();

    if (!m_pendingPeers.contains(peerId))
        return;

    if (resource->getStatus() == Qn::Offline)
        m_stoppingPeers.remove(peerId);
    else if (!m_stoppingPeers.contains(peerId))
        m_restartingPeers.remove(peerId);

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    if (server->getVersion() == m_version) {
        m_pendingPeers.remove(peerId);
        m_stoppingPeers.remove(peerId);
        m_restartingPeers.remove(peerId);
        emit peerFinished(peerId);
    }

    if (m_pendingPeers.isEmpty()) {
        finish(NoError);
        return;
    }

    if (m_restartingPeers.isEmpty()) {
        /* When all peers were restarted we only have to wait for saveServer transactions.
           It shouldn't take long time. So restart timer to a short timeout. */
        m_checkTimer->start(shortTimeout);
    }
}

void QnInstallUpdatesPeerTask::at_checkTimeout() {
    foreach (const QnUuid &id, m_pendingPeers) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (server->getVersion() == m_version) {
            m_pendingPeers.remove(id);
            emit peerFinished(id);
        }
    }

    finish(m_pendingPeers.isEmpty() ? NoError : InstallationFailed);
}

void QnInstallUpdatesPeerTask::finish(int errorCode) {
    qnResPool->disconnect(this);
    static_cast<QnClientMessageProcessor*>(QnClientMessageProcessor::instance())->setHoldConnection(false);
    QnNetworkPeerTask::finish(errorCode);
}
