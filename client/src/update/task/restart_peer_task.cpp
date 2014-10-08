#include "restart_peer_task.h"

#include <QtCore/QTimer>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace {
    const int timeout = 2 * 60 * 1000;
}

QnRestartPeerTask::QnRestartPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(timeout);
    m_timer->setSingleShot(true);

    connect(m_timer, &QTimer::timeout, this, &QnRestartPeerTask::at_timeout);
}

void QnRestartPeerTask::doStart() {
    m_restartingServers.clear();

    foreach (const QnUuid &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            Q_ASSERT_X(0, "Non-server resource in server task.", Q_FUNC_INFO);

        m_restartingServers.append(server);
    }

    restartNextServer();
}

void QnRestartPeerTask::restartNextServer() {
    m_timer->stop();

    if (m_restartingServers.isEmpty()) {
        finish(NoError);
        return;
    }

    QnMediaServerResourcePtr server = m_restartingServers.first();
    server->apiConnection()->restart(this, SLOT(at_commandSent(int,int)));
}

void QnRestartPeerTask::finishPeer() {
    QnMediaServerResourcePtr server = m_restartingServers.takeFirst();
    emit peerFinished(server->getId());
    restartNextServer();
}

void QnRestartPeerTask::at_commandSent(int status, int handle) {
    Q_UNUSED(handle)

    if (status != 0) {
        finish(RestartError);
        return;
    }

    m_serverWasOffline = false;
    QnMediaServerResourcePtr server = m_restartingServers.first();
    connect(server.data(), &QnMediaServerResource::statusChanged, this, &QnRestartPeerTask::at_resourceStatusChanged);
    m_timer->start();
}

void QnRestartPeerTask::at_resourceStatusChanged() {
    QnMediaServerResourcePtr server = m_restartingServers.first();
    if (server->getStatus() == Qn::Offline) {
        m_serverWasOffline = true;
        return;
    } else {
        if (m_serverWasOffline) {
            disconnect(server.data(), &QnMediaServerResource::statusChanged, this, &QnRestartPeerTask::at_resourceStatusChanged);
            finishPeer();
        }
    }
}

void QnRestartPeerTask::at_timeout() {
    QnMediaServerResourcePtr server = m_restartingServers.first();
    disconnect(server.data(), &QnMediaServerResource::statusChanged, this, &QnRestartPeerTask::at_resourceStatusChanged);
    finish(RestartError);
}
