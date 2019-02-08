#include "install_updates_peer_task.h"

#include <chrono>

#include <QtCore/QTimer>


#include <api/app_server_connection.h>
#include <api/model/upload_update_reply.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <client/client_message_processor.h>
#include <network/router.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/common/delete_later.h>

#include <nx/utils/log/log.h>

namespace {

using namespace std::chrono;
using namespace std::chrono_literals;

constexpr milliseconds kCheckTimeoutMs = 20min;
constexpr milliseconds kStartPingTimeoutMs = 1min;
constexpr milliseconds kPingIntervalMs = 10s;

static const nx::utils::SoftwareVersion kUnauthenticatedUpdateMinVersion(3, 0);

QnInstallUpdatesPeerTask::PeersToQueryMap sortedPeers(QnRouter* router, const QSet<QnUuid>& peers)
{
    QnInstallUpdatesPeerTask::PeersToQueryMap result;

    if (!router)
        return result;

    for (const auto& id: peers)
        result.emplace(router->routeTo(id).distance, id);

    return result;
}

} // namespace;

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

void QnInstallUpdatesPeerTask::setVersion(const nx::utils::SoftwareVersion& version)
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

    connect(resourcePool(), &QnResourcePool::statusChanged,
        this, &QnInstallUpdatesPeerTask::at_resourceStatusChanged);

    m_stoppingPeers = m_restartingPeers = m_pendingPeers = peers();

    m_protoProblemDetected = false;
    qnClientMessageProcessor->setHoldConnection(true);

    m_serverByRequest.clear();

    m_peersToQuery = sortedPeers(commonModule()->router(), peers());

    NX_DEBUG(this, lm("Start installing update [%1].").arg(m_updateId));

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

        NX_VERBOSE(this, lm("Updating server [%1, %2, %3], distance %4.").args(
            server->getName(), server->getApiUrl(), server->getVersion(), distance));

        if (server->getVersion() >= kUnauthenticatedUpdateMinVersion)
        {
            // NOTE: This method is being removed from mediaservers
            NX_ASSERT(false);
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

        connect(server.data(), &QnMediaServerResource::versionChanged,
            this, &QnInstallUpdatesPeerTask::at_serverVersionChanged);
    }
}

void QnInstallUpdatesPeerTask::startWaiting()
{
    m_checkTimer->start(kCheckTimeoutMs.count());
    m_pingTimer->start(kStartPingTimeoutMs.count());

    int peersSize = m_pendingPeers.size();

    auto progressTimer = new QTimer(this);
    progressTimer->setInterval(1000);
    progressTimer->setSingleShot(false);
    connect(progressTimer, &QTimer::timeout,  this,
        [this, peersSize]
        {
            int progress = (kCheckTimeoutMs.count() - m_checkTimer->remainingTime())
                * 100 / kCheckTimeoutMs.count();
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

void QnInstallUpdatesPeerTask::at_resourceStatusChanged(const QnResourcePtr& resource)
{
    const auto peerId = resource->getId();

    /* Stop ping timer if the main server has appeared online */
    if (peerId == m_ecServer->getId()
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
}

void QnInstallUpdatesPeerTask::at_serverVersionChanged(const QnResourcePtr& resource)
{
    const auto peerId = resource->getId();

    const auto server = resource.dynamicCast<QnMediaServerResource>();
    NX_ASSERT(server);

    const bool peerUpdateFinished = server && server->getVersion() == m_version;

    if (peerUpdateFinished)
    {
        m_pendingPeers.remove(peerId);
        m_stoppingPeers.remove(peerId);
        m_restartingPeers.remove(peerId);

        NX_DEBUG(this, lm("Installation finished [%1, %2].").args(
            server->getName(), server->getId()));

        emit peerFinished(peerId);
    }

    if (m_pendingPeers.isEmpty())
    {
        NX_DEBUG(this, "Finished.");
        finish(NoError);
        return;
    }

    if (peerUpdateFinished)
        removeWaitingPeer(peerId);
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

    NX_DEBUG(this, lm("Finished on timeout [%1].").arg(result));

    finish(result);
}

void QnInstallUpdatesPeerTask::at_pingTimer_timeout()
{
    NX_VERBOSE(this, "Ping EC.");

    m_pingTimer->setInterval(kPingIntervalMs.count());

    if (!m_ecConnection)
    {
        m_ecConnection = QnMediaServerConnectionPtr(
            new QnMediaServerConnection(commonModule(), m_ecServer, QnUuid(), true), &qnDeleteLater);
    }

    m_ecConnection->modulesInformation(
        this, SLOT(at_gotModuleInformation(int, QList<nx::vms::api::ModuleInformation>, int)));
}

void QnInstallUpdatesPeerTask::at_gotModuleInformation(
    int status, const QList<nx::vms::api::ModuleInformation>& modules, int handle)
{
    Q_UNUSED(handle)

    if (status != 0)
    {
        NX_VERBOSE(this, "Ping EC failed.");
        return;
    }

    NX_VERBOSE(this, "Ping EC succeed.");

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
            NX_VERBOSE(this, lm("Protocol problem found: %1 != %2.").args(
                moduleInformation.protoVersion, nx_ec::EC2_PROTO_VERSION));
        }

        if (server->getVersion() != moduleInformation.version)
        {
            NX_VERBOSE(this, lm("Ping reply: Updating server version: %1 [%2].").args(
                moduleInformation.id, moduleInformation.version));

            server->setVersion(moduleInformation.version);
            at_serverVersionChanged(server);
        }
    }
}

void QnInstallUpdatesPeerTask::at_installUpdateResponse(
    int status, const QnUploadUpdateReply& reply, int handle)
{
    const auto server = m_serverByRequest.take(handle);
    if (!server)
        return;

    NX_VERBOSE(this, lm("Reply [status = %1, reply = %2, server: %3, %4].").args(
        status, reply.offset, server->getName(), server->getApiUrl()));

    if (status != 0 || reply.offset != ec2::AbstractUpdatesManager::NoError)
    {
        finish(InstallationFailed, { server->getId() });
        return;
    }

    if (m_serverByRequest.isEmpty())
        queryNextGroup();
}
