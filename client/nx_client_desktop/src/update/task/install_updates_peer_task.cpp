#include "install_updates_peer_task.h"

#include <chrono>

#include <QtCore/QTimer>

#include <common/common_module.h>

#include <api/app_server_connection.h>
#include <api/model/upload_update_reply.h>
#include <nx_ec/ec_proto_version.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <client/client_message_processor.h>
#include <utils/common/delete_later.h>
#include <nx/utils/log/log.h>
#include <network/router.h>

namespace detail {
    using namespace std::chrono;

    const int kCheckTimeoutMs = milliseconds(minutes(20)).count();
    const int kStartPingTimeoutMs = milliseconds(minutes(1)).count();
    const int kPingIntervalMs = milliseconds(seconds(10)).count();
    const int kShortTimeoutMs = milliseconds(minutes(1)).count();

    const QnSoftwareVersion kUnauthenticatedUpdateMinVersion(3, 0);

    QnInstallUpdatesPeerTask::PeersToQueryMap sortedPeers(QnRouter* router, const QSet<QnUuid>& peers)
    {
        QnInstallUpdatesPeerTask::PeersToQueryMap result;

        if (!router)
            return result;

        for (const auto& id: peers)
            result.emplace(router->routeTo(id).distance, id);

        return result;
    }

} using namespace detail;

QnInstallUpdatesPeerTask::QnInstallUpdatesPeerTask(QObject* parent):
    QnNetworkPeerTask(parent)
{
    m_checkTimer = new QTimer(this);
    m_checkTimer->setSingleShot(true);

    m_pingTimer = new QTimer(this);

    connect(m_checkTimer, &QTimer::timeout,
        this, &QnInstallUpdatesPeerTask::at_checkTimer_timeout);
    connect(m_pingTimer, &QTimer::timeout,
        this, &QnInstallUpdatesPeerTask::at_pingTimer_timeout);
}

void QnInstallUpdatesPeerTask::setUpdateId(const QString& updateId)
{
    m_updateId = updateId;
}

void QnInstallUpdatesPeerTask::setVersion(const QnSoftwareVersion& version)
{
    m_version = version;
}

void QnInstallUpdatesPeerTask::finish(int errorCode, const QSet<QnUuid>& failedPeers)
{
    resourcePool()->disconnect(this);
    m_ecConnection.reset();
    m_checkTimer->stop();
    m_serverByRequest.clear();
    /* There's no need to unhold connection now if we can't connect to the server */
    if (!m_protoProblemDetected)
        qnClientMessageProcessor->setHoldConnection(false);
    QnNetworkPeerTask::finish(errorCode, failedPeers);
}

void QnInstallUpdatesPeerTask::doStart()
{
    if (peers().isEmpty())
    {
        finish(NoError);
        return;
    }

    m_ecServer = commonModule()->currentServer();

    connect(resourcePool(), &QnResourcePool::resourceChanged,
        this, &QnInstallUpdatesPeerTask::at_resourceChanged);

    m_stoppingPeers = m_restartingPeers = m_pendingPeers = peers();

    m_protoProblemDetected = false;
    qnClientMessageProcessor->setHoldConnection(true);

    m_serverByRequest.clear();

    m_peersToQuery = sortedPeers(commonModule()->router(), peers());

    NX_LOG(lit("Update: QnInstallUpdatesPeerTask: Start installing update [%1].").arg(m_updateId),
        cl_logDEBUG1);

    queryNextGroup();
}

void QnInstallUpdatesPeerTask::queryNextGroup()
{
    if (m_peersToQuery.empty())
    {
        startWaiting();
        return;
    }

    auto it = m_peersToQuery.begin();
    const auto distance = it->first;

    while (!m_peersToQuery.empty() && it->first == distance)
    {
        const auto id = it->second;
        it = m_peersToQuery.erase(it);

        const auto server = resourcePool()->getResourceById<QnMediaServerResource>(id);
        if (!server)
        {
            finish(InstallationFailed, { id });
            return;
        }

        int handle = -1;

        NX_LOG(
            lit("Update: QnInstallUpdatePeerTask: Updating server [%1, %2, %3], distance %4.")
                .arg(server->getName())
                .arg(server->getApiUrl().toString())
                .arg(server->getVersion().toString())
                .arg(distance),
            cl_logDEBUG2);

        if (server->getVersion() >= kUnauthenticatedUpdateMinVersion)
        {
            handle = server->apiConnection()->installUpdateUnauthenticated(
                m_updateId, true,
                this, SLOT(at_installUpdateResponse(int,QnUploadUpdateReply,int)));
        }
        else
        {
            handle = server->apiConnection()->installUpdate(
                m_updateId, true,
                this, SLOT(at_installUpdateResponse(int,QnUploadUpdateReply,int)));
        }

        if (handle < 0)
        {
            finish(InstallationFailed, { id });
            return;
        }

        m_serverByRequest.insert(handle, server);
    }
}

void QnInstallUpdatesPeerTask::startWaiting()
{
    m_checkTimer->start(kCheckTimeoutMs);
    m_pingTimer->start(kStartPingTimeoutMs);

    int peersSize = m_pendingPeers.size();

    auto progressTimer = new QTimer(this);
    progressTimer->setInterval(1000);
    progressTimer->setSingleShot(false);
    connect(progressTimer, &QTimer::timeout,  this,
        [this, peersSize]
        {
            int progress =
                (kCheckTimeoutMs - m_checkTimer->remainingTime()) * 100 / kCheckTimeoutMs;
            progress = qBound(0, progress, 100);

            /* Count finished peers. */
            int totalProgress = (peersSize - m_pendingPeers.size()) * 100;

            for (const auto& peerId: m_pendingPeers)
            {
                int peerProgress = progress;
                /* Peer already restarted. */
                if (!m_restartingPeers.contains(peerId))
                    peerProgress = qMax(progress, 99);
                /* Peer already stopped. */
                else if (!m_stoppingPeers.contains(peerId))
                    peerProgress = qMax(progress, 95);
                emit peerProgressChanged(peerId, peerProgress);

                /* Count processing peers. */
                totalProgress += peerProgress;
            }

            emit progressChanged(totalProgress / peersSize);
        });

    connect(this, &QnInstallUpdatesPeerTask::finished, progressTimer, &QTimer::stop);
    progressTimer->start();
}

void QnInstallUpdatesPeerTask::removeWaitingPeer(const QnUuid& id)
{
    auto it = std::find_if(m_serverByRequest.begin(), m_serverByRequest.end(),
        [&id](const QnMediaServerResourcePtr& server) { return server->getId() == id; });

    if (it == m_serverByRequest.end())
        return;

    m_serverByRequest.erase(it);

    if (m_serverByRequest.isEmpty())
        queryNextGroup();
}

void QnInstallUpdatesPeerTask::at_resourceChanged(const QnResourcePtr& resource)
{
    QnUuid peerId = resource->getId();

    /* Stop ping timer if the main server has appeared online */
    if (resource->getId() == m_ecServer->getId()
        && !m_stoppingPeers.contains(peerId)
        && resource->getStatus() == Qn::Online)
    {
        m_pingTimer->stop();
    }

    if (!m_pendingPeers.contains(peerId))
        return;

    if (resource->getStatus() == Qn::Offline)
        m_stoppingPeers.remove(peerId);
    else if (!m_stoppingPeers.contains(peerId))
        m_restartingPeers.remove(peerId);

    const auto server = resource.dynamicCast<QnMediaServerResource>();
    NX_ASSERT(server);

    const bool peerUpdateFinished = server && server->getVersion() == m_version;

    if (peerUpdateFinished)
    {
        m_pendingPeers.remove(peerId);
        m_stoppingPeers.remove(peerId);
        m_restartingPeers.remove(peerId);

        NX_LOG(lit("Update: QnInstallUpdatesPeerTask: Installation finished [%1, %2].")
           .arg(server->getName()).arg(server->getId().toString()), cl_logDEBUG1);

        emit peerFinished(peerId);
    }

    if (m_pendingPeers.isEmpty())
    {
        NX_LOG(lit("Update: QnInstallUpdatesPeerTask: Finished."), cl_logDEBUG1);
        finish(NoError);
        return;
    }

    if (peerUpdateFinished)
        removeWaitingPeer(peerId);

    if (m_restartingPeers.isEmpty())
    {
        /* When all peers were restarted we only have to wait for saveServer transactions.
           It shouldn't take long time. So restart timer to a short timeout. */
        m_checkTimer->start(kShortTimeoutMs);
    }
}

void QnInstallUpdatesPeerTask::at_checkTimer_timeout()
{
    auto peers = m_pendingPeers; // Copy to safely remove elements from m_pendingPeers.
    for (const auto& id: peers)
    {
        const auto server = resourcePool()->getIncompatibleServerById(id, true);
        if (!server)
            continue;

        if (server->getVersion() == m_version)
        {
            m_pendingPeers.remove(id);
            emit peerFinished(id);
            removeWaitingPeer(id);
        }
    }

    int result = m_pendingPeers.isEmpty() ? NoError : InstallationFailed;

    NX_LOG(lit("Update: QnInstallUpdatesPeerTask: Finished on timeout [%1].").arg(result),
        cl_logDEBUG1);

    finish(result);
}

void QnInstallUpdatesPeerTask::at_pingTimer_timeout()
{
    NX_LOG(lit("Update: QnInstallUpdatesPeerTask: Ping EC."), cl_logDEBUG2);

    m_pingTimer->setInterval(kPingIntervalMs);

    if (!m_ecConnection)
    {
        m_ecConnection = QnMediaServerConnectionPtr(
            new QnMediaServerConnection(commonModule(), m_ecServer, QnUuid(), true), &qnDeleteLater);
    }

    m_ecConnection->modulesInformation(
        this, SLOT(at_gotModuleInformation(int,QList<QnModuleInformation>,int)));
}

void QnInstallUpdatesPeerTask::at_gotModuleInformation(
    int status, const QList<QnModuleInformation>& modules, int handle)
{
    Q_UNUSED(handle)

    if (status != 0)
    {
        NX_LOG(lit("Update: QnInstallUpdatesPeerTask: Ping EC failed."), cl_logDEBUG2);
        return;
    }

    NX_LOG(lit("Update: QnInstallUpdatesPeerTask: Ping EC succeed."), cl_logDEBUG2);

    for (const auto& moduleInformation: modules)
    {
        const auto server =
            resourcePool()->getResourceById<QnMediaServerResource>(moduleInformation.id);
        if (!server)
            continue;

        if (!m_protoProblemDetected && moduleInformation.protoVersion != nx_ec::EC2_PROTO_VERSION)
        {
            m_protoProblemDetected = true;
            emit protocolProblemDetected();
            NX_LOG(
                lit("Update: QnInstallUpdatesPeerTask: Protocol problem found: %1 != %2.")
                    .arg(moduleInformation.protoVersion).arg(nx_ec::EC2_PROTO_VERSION),
                cl_logDEBUG2);
        }

        if (server->getVersion() != moduleInformation.version)
        {
            NX_LOG(
                lit("Update: QnInstallUpdatesPeerTask: "
                    "Ping reply: Updating server version: %1 [%2].")
                    .arg(moduleInformation.id.toString())
                    .arg(moduleInformation.version.toString()),
                cl_logDEBUG2);

            server->setVersion(moduleInformation.version);
            at_resourceChanged(server);
        }
    }
}

void QnInstallUpdatesPeerTask::at_installUpdateResponse(
    int status, const QnUploadUpdateReply& reply, int handle)
{
    const auto server = m_serverByRequest.take(handle);
    if (!server)
        return;

    NX_LOG(
        lit("Update: QnInstallUpdatePeerTask: Reply [status = %1, reply = %2, server: %3, %4].")
            .arg(status)
            .arg(reply.offset)
            .arg(server->getName())
            .arg(server->getApiUrl().toString()),
        cl_logDEBUG2);

    if (status != 0 || reply.offset != ec2::AbstractUpdatesManager::NoError)
    {
        finish(InstallationFailed, { server->getId() });
        return;
    }

    if (m_serverByRequest.isEmpty())
        queryNextGroup();
}
