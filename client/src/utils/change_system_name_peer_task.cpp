#include "change_system_name_peer_task.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/global_module_finder.h>

QnChangeSystemNamePeerTask::QnChangeSystemNamePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_rebootPeers(true)
{
}

QString QnChangeSystemNamePeerTask::systemName() const {
    return m_systemName;
}

void QnChangeSystemNamePeerTask::setSystemName(const QString &systemName) {
    m_systemName = systemName;
}

void QnChangeSystemNamePeerTask::setRebootPeers(bool rebootPeers) {
    m_rebootPeers = rebootPeers;
}

void QnChangeSystemNamePeerTask::doStart() {
    foreach (const QnId &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server) {
            m_failedPeers.insert(id);
            continue;
        }

        int handle = server->apiConnection()->changeSystemNameAsync(m_systemName, m_rebootPeers, this, SLOT(processReply(int,int)));
        m_pendingPeers.insert(handle, id);
    }

    if (m_pendingPeers.isEmpty())
        finish(m_failedPeers.isEmpty() ? 0 : 1, m_failedPeers);
}

void QnChangeSystemNamePeerTask::processReply(int status, int handle) {
    QnId id = m_pendingPeers.take(handle);
    if (id.isNull())
        return;

    if (status != 0)
        m_failedPeers.insert(id);

    if (m_pendingPeers.isEmpty())
        finish(0);
}
