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

void QnWaitCompatibleServersPeerTask::doStart() {
    m_targets = peers();

    foreach (const QnUuid &id, m_targets) {
        QnResourcePtr resource = qnResPool->getResourceById(id);
        if (resource && resource->getStatus() != Qn::Offline)
            m_targets.remove(id);
    }

    if (m_targets.isEmpty()) {
        finish(NoError);
        return;
    }

    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnWaitCompatibleServersPeerTask::at_resourcePool_resourceChanged);
    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnWaitCompatibleServersPeerTask::at_resourcePool_resourceChanged);
    m_timer->start();
}

void QnWaitCompatibleServersPeerTask::at_resourcePool_resourceChanged(const QnResourcePtr &resource) {
    QnUuid id = resource->getId();

    if (m_targets.contains(id) && resource->getStatus() != Qn::Offline)
        m_targets.remove(id);

    if (m_targets.isEmpty())
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
