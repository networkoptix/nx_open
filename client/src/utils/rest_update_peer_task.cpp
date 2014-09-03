#include "rest_update_peer_task.h"

#include <QtCore/QTimer>

#include <core/resource_management/resource_pool.h>

namespace {
    const int shortTimeout = 60 * 1000;
    const int longTimeout = 5 * 60 * 1000;
}

QnRestUpdatePeerTask::QnRestUpdatePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent)
{
    m_shortTimer = new QTimer(this);
    m_shortTimer->setInterval(shortTimeout);
    m_shortTimer->setSingleShot(true);
    m_longTimer = new QTimer(this);
    m_longTimer->setInterval(longTimeout);
    m_longTimer->setSingleShot(true);

    connect(m_shortTimer,       &QTimer::timeout,       this,   &QnRestUpdatePeerTask::at_shortTimeout);
    connect(m_longTimer,        &QTimer::timeout,       this,   &QnRestUpdatePeerTask::at_longTimeout);

    // after finish we ought to do some cleanup
    connect(this,   &QnRestUpdatePeerTask::finished,    this,   &QnRestUpdatePeerTask::at_finished);
}

void QnRestUpdatePeerTask::setUpdateFiles(const QHash<QnSystemInformation, QString> &updateFiles) {
    m_updateFiles = updateFiles;
}

void QnRestUpdatePeerTask::setUpdateId(const QString &updateId) {
    m_updateId = updateId;
}

void QnRestUpdatePeerTask::setVersion(const QnSoftwareVersion &version) {
    m_version = version;
}

void QnRestUpdatePeerTask::cancel() {
    if (!m_currentServers.isEmpty()) {
        QnMediaServerResourcePtr server = m_currentServers.first();
        disconnect(server.data(), &QnMediaServerResource::resourceChanged, this, &QnRestUpdatePeerTask::at_resourceChanged);
    }

    m_serverBySystemInformation.clear();
    m_currentServers.clear();
    m_currentData.clear();
    m_shortTimer->stop();
    m_longTimer->stop();
}

void QnRestUpdatePeerTask::doStart() {
    foreach (const QUuid &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            Q_ASSERT_X(0, "Non-server resource in server task.", Q_FUNC_INFO);

        if (!m_updateFiles.contains(server->getSystemInfo())) {
            finish(ParametersError);
            return;
        }

        m_serverBySystemInformation.insert(server->getSystemInfo(), server);
    }

    installNextUpdate();
}

void QnRestUpdatePeerTask::installNextUpdate() {
    m_shortTimer->stop();
    m_longTimer->stop();

    if (m_currentServers.isEmpty()) {
        if (m_serverBySystemInformation.isEmpty()) {
            finish(NoError);
            return;
        }

        auto it = m_serverBySystemInformation.begin();

        QnSystemInformation systemInformation = it.key();
        m_currentServers = m_serverBySystemInformation.values(systemInformation);

        QFile file(m_updateFiles[systemInformation]);
        if (!file.open(QFile::ReadOnly)) {
            finish(FileError);
            return;
        }
        m_currentData = file.readAll();
        file.close();
    }

    QnMediaServerResourcePtr server = m_currentServers.first();
    m_targetId = QUuid(server->getProperty(lit("guid")));
    server->apiConnection()->installUpdate(m_updateId, m_currentData, this, SLOT(at_updateInstalled(int,int)));
}

void QnRestUpdatePeerTask::finishPeer() {
    QnMediaServerResourcePtr server = m_currentServers.takeFirst();
    emit peerFinished(server->getId());
    if (m_currentServers.isEmpty())
        m_serverBySystemInformation.erase(m_serverBySystemInformation.begin());
    installNextUpdate();
}

void QnRestUpdatePeerTask::at_updateInstalled(int status, int handle) {
    Q_UNUSED(handle)

    // it means the task was cancelled
    if (m_currentServers.isEmpty())
        return;

    if (status != 0) {
        finish(UploadError);
        return;
    }

    if (m_targetId.isNull()) {
        QnMediaServerResourcePtr server = m_currentServers.first();
        connect(server.data(), &QnMediaServerResource::resourceChanged, this, &QnRestUpdatePeerTask::at_resourceChanged);
    } else {
        connect(qnResPool, &QnResourcePool::resourceChanged, this, &QnRestUpdatePeerTask::at_resourceChanged);
    }
    m_shortTimer->start();
    m_longTimer->start();
}

void QnRestUpdatePeerTask::at_resourceChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = m_currentServers.first();
    if (!m_targetId.isNull()) {
        if (m_targetId != resource->getId())
            return;

        server = qnResPool->getResourceById(m_targetId).dynamicCast<QnMediaServerResource>();
        if (!server)
            return;
    }

    if (server->getVersion() == m_version) {
        sender()->disconnect(this);
        finishPeer();
    }
}

void QnRestUpdatePeerTask::at_shortTimeout() {
    /* We suppose that after the short timeout the update must be started.
     * If the server is offline we'll wait more till long timeout.
     * Otherwise the update has been failed :(
     */
    QnMediaServerResourcePtr server = m_currentServers.first();
    if (server->getStatus() == Qn::Offline)
        return;

    if (server->getVersion() != m_version) {
        finish(InstallationError);
        return;
    }

    disconnect(server.data(), &QnMediaServerResource::resourceChanged, this, &QnRestUpdatePeerTask::at_resourceChanged);
    finishPeer();
}

void QnRestUpdatePeerTask::at_longTimeout() {
    /* After the long timeout the server must finish the upgrade process. */
    QnMediaServerResourcePtr server = m_currentServers.first();
    disconnect(server.data(), &QnMediaServerResource::resourceChanged, this, &QnRestUpdatePeerTask::at_resourceChanged);

    if (server->getVersion() != m_version) {
        finish(InstallationError);
        return;
    }

    finishPeer();
}

void QnRestUpdatePeerTask::at_finished() {
    m_currentData.clear();
}
