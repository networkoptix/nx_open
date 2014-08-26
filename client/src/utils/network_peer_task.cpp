#include "network_peer_task.h"

QnNetworkPeerTask::QnNetworkPeerTask(QObject *parent) :
    QObject(parent)
{
}

QnNetworkPeerTask::~QnNetworkPeerTask() {}

void QnNetworkPeerTask::start(const QSet<QUuid> &peers) {
    m_peers = peers;
    doStart();
}

QSet<QUuid> QnNetworkPeerTask::peers() const {
    return m_peers;
}

void QnNetworkPeerTask::setPeers(const QSet<QUuid> &peers) {
    m_peers = peers;
}

void QnNetworkPeerTask::start() {
    doStart();
}

void QnNetworkPeerTask::finish(int errorCode, const QSet<QUuid> &failedPeers) {
    emit finished(errorCode, failedPeers);
}
