#include "install_updates_peer_task.h"

#include <QtCore/QTimer>

#include <api/app_server_connection.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/ec_proto_version.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <utils/common/delete_later.h>

namespace {
    const int checkTimeout = 15 * 60 * 1000;
    const int startPingTimeout = 1 * 60 * 1000;
    const int pingInterval = 10 * 1000;
    const int shortTimeout = 60 * 1000;

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }
}

QnInstallUpdatesPeerTask::QnInstallUpdatesPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_protoProblemDetected(false)
{
    m_checkTimer = new QTimer(this);
    m_checkTimer->setSingleShot(true);

    m_pingTimer = new QTimer(this);

    connect(m_checkTimer,   &QTimer::timeout,                       this,           &QnInstallUpdatesPeerTask::at_checkTimer_timeout);
    connect(m_pingTimer,    &QTimer::timeout,                       this,           &QnInstallUpdatesPeerTask::at_pingTimer_timeout);
    connect(this,           &QnInstallUpdatesPeerTask::finished,    m_checkTimer,   &QTimer::stop);
}

void QnInstallUpdatesPeerTask::setUpdateId(const QString &updateId) {
    m_updateId = updateId;
}

void QnInstallUpdatesPeerTask::setVersion(const QnSoftwareVersion &version) {
    m_version = version;
}

void QnInstallUpdatesPeerTask::finish(int errorCode) {
    qnResPool->disconnect(this);
    m_ecConnection.reset();
    /* There's no need to unhold connection now if we can't connect to the server */
    if (!m_protoProblemDetected)
        static_cast<QnClientMessageProcessor*>(QnClientMessageProcessor::instance())->setHoldConnection(false);
    QnNetworkPeerTask::finish(errorCode);
}

void QnInstallUpdatesPeerTask::doStart() {
    if (peers().isEmpty()) {
        finish(NoError);
        return;
    }

    m_ecServer = qnResPool->getResourceById(qnCommon->remoteGUID()).dynamicCast<QnMediaServerResource>();

    connect(qnResPool, &QnResourcePool::resourceChanged, this, &QnInstallUpdatesPeerTask::at_resourceChanged);

    m_stoppingPeers = m_restartingPeers = m_pendingPeers = peers();

    m_protoProblemDetected = false;
    static_cast<QnClientMessageProcessor*>(QnClientMessageProcessor::instance())->setHoldConnection(true);

    connection2()->getUpdatesManager()->installUpdate(m_updateId, m_pendingPeers,
                                                      this, [this](int, ec2::ErrorCode){});
    m_checkTimer->start(checkTimeout);
    m_pingTimer->start(startPingTimeout);

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

    /* Stop ping timer if the main server has appeared online */
    if (resource->getId() == m_ecServer->getId() && !m_stoppingPeers.contains(peerId) && resource->getStatus() == Qn::Online)
        m_pingTimer->stop();

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

void QnInstallUpdatesPeerTask::at_checkTimer_timeout() {
    foreach (const QnUuid &id, m_pendingPeers) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (server->getVersion() == m_version) {
            m_pendingPeers.remove(id);
            emit peerFinished(id);
        }
    }

    finish(m_pendingPeers.isEmpty() ? NoError : InstallationFailed);
}

void QnInstallUpdatesPeerTask::at_pingTimer_timeout() {
    m_pingTimer->setInterval(pingInterval);

    if (!m_ecConnection)
        m_ecConnection = QnMediaServerConnectionPtr(new QnMediaServerConnection(m_ecServer.data(), QnUuid(), true), &qnDeleteLater);

    m_ecConnection->modulesInformation(this, SLOT(at_gotModuleInformation(int,QList<QnModuleInformation>,int)));
}

void QnInstallUpdatesPeerTask::at_gotModuleInformation(int status, const QList<QnModuleInformation> &modules, int handle) {
    Q_UNUSED(handle)

    if (status != 0)
        return;

    for (const QnModuleInformation &moduleInformation: modules) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(moduleInformation.id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (moduleInformation.protoVersion != nx_ec::EC2_PROTO_VERSION)
            m_protoProblemDetected = true;

        if (server->getVersion() != moduleInformation.version) {
            server->setVersion(moduleInformation.version);
            at_resourceChanged(server);
        }
    }
}
