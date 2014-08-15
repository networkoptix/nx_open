#include "install_updates_peer_task.h"

#include <QtCore/QTimer>

#include <api/app_server_connection.h>
#include <nx_ec/ec_api.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <client/client_message_processor.h>

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

    static_cast<QnClientMessageProcessor*>(QnClientMessageProcessor::instance())->setHoldConnection(true);

    connection2()->getUpdatesManager()->installUpdate(m_updateId, m_pendingPeers,
                                                      this, [this](int, ec2::ErrorCode){});
    m_checkTimer->start();
}

void QnInstallUpdatesPeerTask::at_resourceChanged(const QnResourcePtr &resource) {
    QUuid peerId = resource->getId();

    if (!peers().contains(peerId))
        return;

    if (resource->getStatus() == Qn::Offline)
        m_restartingPeers.remove(peerId);

    QnMediaServerResourcePtr server = resource.staticCast<QnMediaServerResource>();

    if (!m_pendingPeers.contains(server->getId()))
        return;

    if (server->getVersion() == m_version) {
        m_pendingPeers.remove(peerId);
        emit peerFinished(peerId);
    }

    if (m_pendingPeers.isEmpty()) {
        finish(NoError);
        return;
    }

    if (m_restartingPeers.isEmpty()) {
        foreach (const QUuid &id, m_pendingPeers) {
            QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).staticCast<QnMediaServerResource>();
            if (server->getStatus() == Qn::Offline) {
                return;
            } else if (server->getVersion() != m_version) {
                finish(InstallationFailed);
                return;
            }
        }
        finish(NoError);
    }
}

void QnInstallUpdatesPeerTask::at_checkTimeout() {
    foreach (const QUuid &id, m_pendingPeers) {
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
