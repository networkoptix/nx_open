#include "network_peer_task.h"

QnNetworkPeerTask::QnNetworkPeerTask(QObject *parent) :
    QObject(parent)
{
}

QnNetworkPeerTask::~QnNetworkPeerTask() {}

void QnNetworkPeerTask::start(const QSet<QUuid> &peers) {
    m_peers = peers;
    QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
}

void QnNetworkPeerTask::cancel() {
    QMetaObject::invokeMethod(this, "doCancel", Qt::QueuedConnection);
}

QSet<QUuid> QnNetworkPeerTask::peers() const {
    return m_peers;
}

void QnNetworkPeerTask::finish(int errorCode, const QSet<QUuid> &failedPeers) {
    emit finished(errorCode, failedPeers);
}
