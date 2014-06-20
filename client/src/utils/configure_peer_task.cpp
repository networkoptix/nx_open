#include "configure_peer_task.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/global_module_finder.h>

QnConfigurePeerTask::QnConfigurePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_wholeSystem(false),
    m_port(0)
{
}

QString QnConfigurePeerTask::systemName() const {
    return m_systemName;
}

void QnConfigurePeerTask::setSystemName(const QString &systemName) {
    m_systemName = systemName;
}

int QnConfigurePeerTask::port() const {
    return m_port;
}

void QnConfigurePeerTask::setPort(int port) {
    m_port = port;
}

QString QnConfigurePeerTask::password() const {
    return m_password;
}

void QnConfigurePeerTask::setPassword(const QString &password) {
    m_password = password;
}

QByteArray QnConfigurePeerTask::passwordHash() const {
    return m_passwordHash;
}

QByteArray QnConfigurePeerTask::passwordDigest() const {
    return m_passwordDigest;
}

void QnConfigurePeerTask::setPasswordHash(const QByteArray &hash, const QByteArray &digest) {
    m_passwordHash = hash;
    m_passwordDigest = digest;
}

bool QnConfigurePeerTask::wholeSystem() const {
    return m_wholeSystem;
}

void QnConfigurePeerTask::setWholeSystem(bool wholeSystem) {
    m_wholeSystem = wholeSystem;
}

void QnConfigurePeerTask::doStart() {
    foreach (const QnId &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server) {
            m_failedPeers.insert(id);
            continue;
        }

        int handle = server->apiConnection()->configureAsync(m_wholeSystem, m_systemName, m_password, m_passwordHash, m_passwordDigest, m_port, this, SLOT(processReply(int,int)));
        m_pendingPeers.insert(handle, id);
    }

    if (m_pendingPeers.isEmpty())
        finish(m_failedPeers.isEmpty() ? 0 : 1, m_failedPeers);
}

void QnConfigurePeerTask::processReply(int status, int handle) {
    QnId id = m_pendingPeers.take(handle);
    if (id.isNull())
        return;

    if (status != 0)
        m_failedPeers.insert(id);

    if (m_pendingPeers.isEmpty())
        finish(0);
}
