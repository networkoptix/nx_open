#include "rest_update_peer_task.h"

#include <QtCore/QTimer>

#include <core/resource_management/resource_pool.h>

namespace {
    const int checkTimeout = 10 * 60 * 1000;
    const int shortTimeout = 5 * 1000;
}

QnRestUpdatePeerTask::QnRestUpdatePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);

    connect(m_timer, &QTimer::timeout, this, &QnRestUpdatePeerTask::at_timer_timeout);

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
    m_timer->stop();
}

void QnRestUpdatePeerTask::doStart() {
    m_currentServers.clear();
    m_serverBySystemInformation.clear();

    foreach (const QUuid &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        Q_ASSERT_X(server, "An incompatible server resource is expected here.", Q_FUNC_INFO);

        if (!m_updateFiles.contains(server->getSystemInfo())) {
            finish(ParametersError);
            return;
        }

        m_serverBySystemInformation.insert(server->getSystemInfo(), server);
    }

    installNextUpdate();
}

void QnRestUpdatePeerTask::installNextUpdate() {
    m_timer->stop();

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
    Q_ASSERT_X(!m_targetId.isNull(), "Each incompatible server resource should has 'guid' property!", Q_FUNC_INFO);
    server->apiConnection()->installUpdate(m_updateId, m_currentData, this, SLOT(at_updateInstalled(int,int)));
}

void QnRestUpdatePeerTask::finishPeer() {
    qnResPool->disconnect(this);
    QnMediaServerResourcePtr server = m_currentServers.takeFirst();
    emit peerFinished(server->getId());
    emit peerUpdateFinished(server->getId(), QUuid(server->getProperty(lit("guid"))));
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

    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnRestUpdatePeerTask::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnRestUpdatePeerTask::at_resourceChanged);
    m_timer->start(checkTimeout);
}

void QnRestUpdatePeerTask::at_resourceChanged(const QnResourcePtr &resource) {
    if (m_currentServers.isEmpty())
        return;

    if (m_targetId != resource->getId())
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    if (server->getStatus() != Qn::Offline && server->getStatus() != Qn::Incompatible) {
        /* The situation is the same as in QnInstallUpdatesPeerTask.
           If the server has gone online we should get resource update soon, so we don't have to wait minutes before we know the new version. */
        m_timer->start(shortTimeout);

        if (server->getVersion() == m_version)
            finishPeer();
    }
}

void QnRestUpdatePeerTask::at_timer_timeout() {
    if (m_currentServers.isEmpty())
        return;

    finish(InstallationError);
}

void QnRestUpdatePeerTask::at_finished() {
    m_currentData.clear();
}
