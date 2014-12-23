#include "rest_update_peer_task.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/ec_proto_version.h>

namespace {
    const int checkTimeout = 15 * 60 * 1000;
    const int shortTimeout = 60 * 1000;
}

QnRestUpdatePeerTask::QnRestUpdatePeerTask(QObject *parent) :
    QnNetworkPeerTask(parent)
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);

    connect(m_timer, &QTimer::timeout, this, &QnRestUpdatePeerTask::at_timer_timeout);

    connect(this, &QnRestUpdatePeerTask::finished, this, [this]() {
        disconnect(qnResPool, NULL, this, NULL);
    });
}

void QnRestUpdatePeerTask::setUpdateId(const QString &updateId) {
    m_updateId = updateId;
}

void QnRestUpdatePeerTask::setVersion(const QnSoftwareVersion &version) {
    m_version = version;
}

void QnRestUpdatePeerTask::doCancel() {
    m_timer->stop();
    m_serverByRealId.clear();
    m_serverByRequest.clear();
}

void QnRestUpdatePeerTask::doStart() {
    foreach (const QnUuid &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        Q_ASSERT_X(server, "An incompatible server resource is expected here.", Q_FUNC_INFO);

        int handle = server->apiConnection()->installUpdate(m_updateId, this, SLOT(at_updateInstalled(int,int)));
        m_serverByRequest[handle] = server;
        m_serverByRealId.insert(QnUuid::fromStringSafe(server->getProperty(lit("guid"))), server);
    }

    if (m_serverByRequest.isEmpty()) {
        finish(NoError);
        return;
    }

    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnRestUpdatePeerTask::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnRestUpdatePeerTask::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged,     this,   &QnRestUpdatePeerTask::at_resourceChanged);
    m_timer->start(checkTimeout);
}

void QnRestUpdatePeerTask::finishPeer(const QnUuid &id) {
    QnMediaServerResourcePtr server = m_serverByRealId.take(id);
    if (!server)
        return;

    emit peerFinished(server->getId());
    emit peerUpdateFinished(server->getId(), QnUuid(server->getProperty(lit("guid"))));

    if (m_serverByRealId.isEmpty())
        finish(NoError);
}

void QnRestUpdatePeerTask::at_updateInstalled(int status, int handle) {
    Q_UNUSED(handle)

    if (m_serverByRealId.isEmpty())
        return;

    QnMediaServerResourcePtr server = m_serverByRequest.take(handle);
    if (!server)
        return;

    if (status != 0) {
        finish(UploadError, QSet<QnUuid>() << server->getId());
        return;
    }
}

void QnRestUpdatePeerTask::at_resourceChanged(const QnResourcePtr &resource) {
    if (m_serverByRealId.isEmpty())
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QnUuid id = QnUuid::fromStringSafe(server->getProperty(lit("guid")));
    if (id.isNull())
        id = server->getId();

    if (!m_serverByRealId.contains(id))
        return;

    Qn::ResourceStatus status = server->getStatus();

    if (status == Qn::Offline)
        return;

    if (server->getVersion() != m_version) {
        if (status == Qn::Online || status == Qn::Unauthorized) {
            /* The situation is the same as in QnInstallUpdatesPeerTask.
             * If the server has gone online we should get resource update soon, so we don't have to wait minutes before we know the new version. */
            m_timer->start(shortTimeout);
        }
        return;
    }

    /* Keep waiting the server to be connected if its proto version matches our but it's still incompatible. */
    if (status == Qn::Incompatible && server->getModuleInformation().protoVersion == nx_ec::EC2_PROTO_VERSION)
        return;

    finishPeer(id);
}

void QnRestUpdatePeerTask::at_timer_timeout() {
    if (m_serverByRealId.isEmpty())
        return;

    finish(InstallationError, QSet<QnUuid>::fromList(m_serverByRealId.keys()));
}
