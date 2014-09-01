#include "wait_compatible_servers_peer_task.h"

#include <QtCore/QTimer>

#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "common/common_module.h"

namespace {

const int timeout = 2 * 60 * 1000; // 2 min

} // anonymous namespace

QnWaitCompatibleServersPeerTask::QnWaitCompatibleServersPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_timer(new QTimer(this))
{
    m_timer->setInterval(timeout);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &QnWaitCompatibleServersPeerTask::at_timer_timeout);
}

void QnWaitCompatibleServersPeerTask::setTargets(const QHash<QUuid, QUuid> &uuids) {
    m_targets = uuids;
}

QHash<QUuid, QUuid> QnWaitCompatibleServersPeerTask::targets() const {
    return m_targets;
}

void QnWaitCompatibleServersPeerTask::doStart() {
    m_realTargets.clear();

    foreach (const QUuid &id, m_targets.keys()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QUuid originalId = m_targets.value(id);
        if (!originalId.isNull())
            m_realTargets.insert(originalId);
    }

    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnWaitCompatibleServersPeerTask::at_resourcePool_resourceAdded);
    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnWaitCompatibleServersPeerTask::at_resourcePool_resourceAdded);
    m_timer->start();
}

void QnWaitCompatibleServersPeerTask::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QUuid id = server->getId();

    if (m_realTargets.contains(id) && server->getSystemName() == qnCommon->localSystemName())
        m_realTargets.remove(id);

    if (m_realTargets.isEmpty())
        finishTask(NoError);
}

void QnWaitCompatibleServersPeerTask::at_timer_timeout() {
    finishTask(TimeoutError);
}

void QnWaitCompatibleServersPeerTask::finishTask(int errorCode) {
    qnResPool->disconnect(this);
    m_timer->stop();
    finish(errorCode);
}
