#include "rest_update_peer_task.h"

#include <QtCore/QTimer>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/ec_proto_version.h>
#include <api/model/upload_update_reply.h>
#include <nx/utils/log/log.h>

namespace {
    const int checkTimeout = 5 * 60 * 1000;
} // namespace

QnRestUpdatePeerTask::QnRestUpdatePeerTask(QObject* parent):
    QnNetworkPeerTask(parent),
    m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);

    connect(m_timer, &QTimer::timeout, this, &QnRestUpdatePeerTask::at_timer_timeout);

    connect(this, &QnRestUpdatePeerTask::finished, this,
        [this]() { disconnect(resourcePool(), nullptr, this, nullptr); });
}

void QnRestUpdatePeerTask::setUpdateId(const QString& updateId)
{
    m_updateId = updateId;
}

void QnRestUpdatePeerTask::setVersion(const QnSoftwareVersion& version)
{
    m_version = version;
}

void QnRestUpdatePeerTask::doCancel()
{
    m_timer->stop();
    m_serverByRealId.clear();
    m_serverByRequest.clear();
}

void QnRestUpdatePeerTask::doStart()
{
    NX_LOG(lit("Update: QnRestUpdatePeerTask: Starting."), cl_logDEBUG1);

    for (const auto& id: peers())
    {
        QnFakeMediaServerResourcePtr server =
            resourcePool()->getIncompatibleResourceById(id).dynamicCast<QnFakeMediaServerResource>();
        NX_ASSERT(server,
            Q_FUNC_INFO, "An incompatible server resource is expected here.");

        NX_LOG(lit("Update: QnRestUpdatePeerTask: Request [%1, %2, %3].")
           .arg(m_updateId, server->getName(), server->getApiUrl().toString()), cl_logDEBUG2);

        const auto handle = server->apiConnection()->installUpdate(
            m_updateId, true, this, SLOT(at_updateInstalled(int,QnUploadUpdateReply,int)));
        m_serverByRequest[handle] = server;

        const auto originalId = server->getOriginalGuid();
        m_serverByRealId.insert(originalId, server);

        connect(server.data(), &QnMediaServerResource::versionChanged,
            this, &QnRestUpdatePeerTask::at_resourceChanged);

        const auto originalServer = resourcePool()->getResourceById<QnMediaServerResource>(originalId);
        if (originalServer)
        {
            connect(originalServer.data(), &QnMediaServerResource::versionChanged,
                this, &QnRestUpdatePeerTask::at_resourceChanged);
        }
    }

    if (m_serverByRequest.isEmpty())
    {
        finish(NoError);
        return;
    }

    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            const auto server = resource.dynamicCast<QnMediaServerResource>();
            if (!server)
                return;

            if (!m_serverByRealId.contains(server->getOriginalGuid()))
                return;

            connect(server.data(), &QnMediaServerResource::versionChanged,
                this, &QnRestUpdatePeerTask::at_resourceChanged);
        });

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            const auto server = resource.dynamicCast<QnMediaServerResource>();
            if (!server)
                return;

            disconnect(server.data(), nullptr, this, nullptr);
        });

    connect(resourcePool(), &QnResourcePool::resourceChanged,
        this, &QnRestUpdatePeerTask::at_resourceChanged);
    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &QnRestUpdatePeerTask::at_resourceChanged);
    connect(resourcePool(), &QnResourcePool::statusChanged,
        this, &QnRestUpdatePeerTask::at_resourceChanged);

    m_timer->start(checkTimeout);
}

void QnRestUpdatePeerTask::finishPeer(const QnUuid &id)
{
    auto server = m_serverByRealId.take(id);
    if (!server)
        return;

    NX_LOG(lit("Update: QnRestUpdatePeerTask: Installation finished [%1, %2].")
           .arg(server->getName()).arg(server->getApiUrl().toString()), cl_logDEBUG1);

    emit peerFinished(server->getId());
    emit peerUpdateFinished(server->getId(), server->getOriginalGuid());

    if (m_serverByRealId.isEmpty()) {
        NX_LOG(lit("Update: QnRestUpdatePeerTask: Installation finished."), cl_logDEBUG1);
        finish(NoError);
    }
}

void QnRestUpdatePeerTask::at_updateInstalled(
    int status, const QnUploadUpdateReply& reply, int handle)
{
    Q_UNUSED(handle)

    if (m_serverByRealId.isEmpty())
        return;

    const auto server = m_serverByRequest.take(handle);
    if (!server)
        return;

    NX_LOG(lit("Update: QnRestUpdatePeerTask: Reply [%1, %2, %3].")
        .arg(reply.offset).arg(server->getName(), server->getApiUrl().toString()), cl_logDEBUG2);

    if (status != 0 || reply.offset != ec2::AbstractUpdatesManager::NoError)
    {
        finish(UploadError, {server->getId()});
        return;
    }
}

void QnRestUpdatePeerTask::at_resourceChanged(const QnResourcePtr& resource)
{
    if (m_serverByRealId.isEmpty())
        return;

    const auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    auto id = server->getOriginalGuid();

    if (!m_serverByRealId.contains(id))
        return;

    if (server->getStatus() == Qn::Offline)
        return;

    if (server->getVersion() != m_version)
        return;

    finishPeer(id);
}

void QnRestUpdatePeerTask::at_timer_timeout()
{
    if (m_serverByRealId.isEmpty())
        return;

    finish(InstallationError, QSet<QnUuid>::fromList(m_serverByRealId.keys()));
}
