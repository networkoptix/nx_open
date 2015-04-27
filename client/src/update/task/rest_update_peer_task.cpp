#include "rest_update_peer_task.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/ec_proto_version.h>
#include <api/model/upload_update_reply.h>
#include <utils/common/log.h>

namespace {
    const int checkTimeout = 15 * 60 * 1000;

    QnUuid getGuid(const QnResourcePtr &resource) {
        return QnUuid::fromStringSafe(resource->getProperty(lit("guid")));
    }
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
    NX_LOG(lit("Update: QnRestUpdatePeerTask: Starting."), cl_logDEBUG1);

    foreach (const QnUuid &id, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        Q_ASSERT_X(server, "An incompatible server resource is expected here.", Q_FUNC_INFO);

        NX_LOG(lit("Update: QnRestUpdatePeerTask: Request [%1, %2, %3].")
               .arg(m_updateId).arg(server->getName()).arg(server->getApiUrl()), cl_logDEBUG2);

        int handle = server->apiConnection()->installUpdate(m_updateId, this, SLOT(at_updateInstalled(int,QnUploadUpdateReply,int)));
        m_serverByRequest[handle] = server;
        m_serverByRealId.insert(getGuid(server), server);

        connect(server.data(), &QnMediaServerResource::versionChanged, this, &QnRestUpdatePeerTask::at_resourceChanged);
    }

    if (m_serverByRequest.isEmpty()) {
        finish(NoError);
        return;
    }

    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   [this](const QnResourcePtr &resource) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            return;

        if (!m_serverByRealId.contains(getGuid(server)))
            return;

        connect(server.data(), &QnMediaServerResource::versionChanged, this, &QnRestUpdatePeerTask::at_resourceChanged);
    });

    connect(qnResPool,  &QnResourcePool::resourceRemoved,   this,   [this](const QnResourcePtr &resource) {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            return;

        disconnect(server.data(), NULL, this, NULL);
    });

    connect(qnResPool,  &QnResourcePool::resourceChanged,   this,   &QnRestUpdatePeerTask::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::resourceAdded,     this,   &QnRestUpdatePeerTask::at_resourceChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged,     this,   &QnRestUpdatePeerTask::at_resourceChanged);
    m_timer->start(checkTimeout);
}

void QnRestUpdatePeerTask::finishPeer(const QnUuid &id) {
    QnMediaServerResourcePtr server = m_serverByRealId.take(id);
    if (!server)
        return;

    NX_LOG(lit("Update: QnRestUpdatePeerTask: Installation finished [%1, %2].")
           .arg(server->getName()).arg(server->getApiUrl()), cl_logDEBUG1);

    emit peerFinished(server->getId());
    emit peerUpdateFinished(server->getId(), getGuid(server));

    if (m_serverByRealId.isEmpty()) {
        NX_LOG(lit("Update: QnRestUpdatePeerTask: Installation finished."), cl_logDEBUG1);
        finish(NoError);
    }
}

void QnRestUpdatePeerTask::at_updateInstalled(int status, const QnUploadUpdateReply &reply, int handle) {
    Q_UNUSED(handle)

    if (m_serverByRealId.isEmpty())
        return;

    QnMediaServerResourcePtr server = m_serverByRequest.take(handle);
    if (!server)
        return;

    NX_LOG(lit("Update: QnRestUpdatePeerTask: Reply [%1, %2, %3].")
           .arg(reply.offset).arg(server->getName()).arg(server->getApiUrl()), cl_logDEBUG2);

    if (status != 0 || reply.offset != ec2::AbstractUpdatesManager::NoError) {
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

    QnUuid id = getGuid(server);
    if (id.isNull())
        id = server->getId();

    if (!m_serverByRealId.contains(id))
        return;

    if (server->getVersion() != m_version)
        return;

    finishPeer(id);
}

void QnRestUpdatePeerTask::at_timer_timeout() {
    if (m_serverByRealId.isEmpty())
        return;

    finish(InstallationError, QSet<QnUuid>::fromList(m_serverByRealId.keys()));
}
