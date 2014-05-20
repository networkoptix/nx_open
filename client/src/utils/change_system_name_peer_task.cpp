#include "change_system_name_peer_task.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/app_server_connection.h>
#include <utils/network/global_module_finder.h>

namespace {

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }

} // anonymous namespace

QnChangeSystemNamePeerTask::QnChangeSystemNamePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent)
{
}

QString QnChangeSystemNamePeerTask::systemName() const {
    return m_systemName;
}

void QnChangeSystemNamePeerTask::setSystemName(const QString &systemName) {
    m_systemName = systemName;
}

void QnChangeSystemNamePeerTask::doStart() {
    foreach (const QnId &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server) {
            m_failedPeers.insert(id);
            continue;
        }

        m_pendingPeers.insert(server->apiConnection()->changeSystemNameAsync(m_systemName, this, SLOT(processReply(int,int))), id);
    }
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
