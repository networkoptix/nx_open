#include "server_message_processor.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/layout_resource.h>

#include <media_server/server_update_tool.h>
#include <media_server/settings.h>
#include <nx_ec/dummy_handler.h>
#include <network/universal_tcp_listener.h>
#include <nx/utils/log/log.h>

#include "serverutil.h"
#include <transaction/message_bus_adapter.h>
#include <nx/vms/server/event/event_message_bus.h>
#include "settings.h"
#include "nx_ec/data/api_conversion_functions.h"
#include <nx/vms/api/data/connection_data.h>
#include <nx_ec/managers/abstract_server_manager.h>
#include "api/app_server_connection.h"
#include "network/router.h"
#include <nx/vms/discovery/manager.h>

#include <utils/common/app_info.h>
#include "core/resource/storage_resource.h"
#include <nx/network/http/custom_headers.h>
#include "resource_status_watcher.h"
#include <media_server/media_server_module.h>
#include "nx_ec/ec_api.h"

using namespace nx;

QnServerMessageProcessor::QnServerMessageProcessor(QnMediaServerModule* serverModule):
    base_type(serverModule->commonModule()),
    m_serverModule(serverModule)
{
}

void QnServerMessageProcessor::updateResource(const QnResourcePtr& resource,
    ec2::NotificationSource source)
{
    QnCommonMessageProcessor::updateResource(resource, source);
    QnMediaServerResourcePtr ownMediaServer =
        resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());

    if (resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (resource->getParentId() != ownMediaServer->getId())
            resource->addFlags( Qn::foreigner );
    }

    if (resource.dynamicCast<QnMediaServerResource>())
    {
        if (resource->getId() == ownMediaServer->getId())
        {
            vms::api::MediaServerData ownData;
            vms::api::MediaServerData newData;

            ec2::fromResourceToApi(ownMediaServer, ownData);
            ec2::fromResourceToApi(resource.staticCast<QnMediaServerResource>(), newData);

            if (ownData != newData && source == ec2::NotificationSource::Remote)
            {
                // If remote peer send update to our server then ignore it and resend our own data.
                commonModule()->ec2Connection()->getMediaServerManager(Qn::kSystemAccess)->save(
                    ownData,
                    ec2::DummyHandler::instance(),
                    &ec2::DummyHandler::onRequestDone);
                return;
            }
        }
        else
        {
            resource->addFlags(Qn::foreigner);
        }
    }

    QnUuid resId = resource->getId();
    if (QnResourcePtr ownResource = resourcePool()->getResourceById(resId))
        ownResource->update(resource);
    else
        resourcePool()->addResource(resource);

    if (m_delayedOnlineStatus.contains(resId))
    {
        m_delayedOnlineStatus.remove(resId);
        resource->setStatus(Qn::Online);
    }
}

void QnServerMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    QnCommonMessageProcessor::init(connection);
}

void QnServerMessageProcessor::startReceivingLocalNotifications(
    const ec2::AbstractECConnectionPtr& connection)
{
    NX_ASSERT(connection);
    if (m_connection) {
        /* Safety check in case connection will not be deleted instantly. */
        m_connection->stopReceivingNotifications();
        disconnectFromConnection(m_connection);
    }
    m_connection = connection;
    connectToConnection(connection);
}

void QnServerMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr& connection)
{
    base_type::connectToConnection(connection);

    connect(connection->updatesNotificationManager().get(), &ec2::AbstractUpdatesNotificationManager::updateChunkReceived,
        this, &QnServerMessageProcessor::at_updateChunkReceived);
    connect(connection->updatesNotificationManager().get(), &ec2::AbstractUpdatesNotificationManager::updateInstallationRequested,
        this, &QnServerMessageProcessor::at_updateInstallationRequested);

    connect(connection, &ec2::AbstractECConnection::remotePeerUnauthorized,
        this, &QnServerMessageProcessor::at_remotePeerUnauthorized);

    connect(connection->miscNotificationManager().get(),
        &ec2::AbstractMiscNotificationManager::systemIdChangeRequested,
        this,
        [this](const QnUuid& systemId, qint64 sysIdTime, nx::vms::api::Timestamp tranLogTime)
        {
            ConfigureSystemData configSystemData;
            configSystemData.localSystemId = systemId;
            configSystemData.sysIdTime = sysIdTime;
            configSystemData.tranLogTime = tranLogTime;
            configSystemData.wholeSystem = true;
            if (m_connection)
                nx::vms::server::Utils(m_serverModule).configureLocalSystem(configSystemData);
        });
}

void QnServerMessageProcessor::disconnectFromConnection(
    const ec2::AbstractECConnectionPtr& connection)
{
    base_type::disconnectFromConnection(connection);
    connection->updatesNotificationManager()->disconnect(this);
    connection->miscNotificationManager()->disconnect(this);
}

void QnServerMessageProcessor::handleRemotePeerFound(QnUuid peer, vms::api::PeerType peerType)
{
    base_type::handleRemotePeerFound(peer, peerType);
    QnResourcePtr res = resourcePool()->getResourceById(peer);
    if (res)
        res->setStatus(Qn::Online);
    else
        m_delayedOnlineStatus << peer;
}

void QnServerMessageProcessor::handleRemotePeerLost(QnUuid peer, vms::api::PeerType peerType)
{
    base_type::handleRemotePeerLost(peer, peerType);
    QnResourcePtr res = resourcePool()->getResourceById(peer);
    if (res) {
        res->setStatus(Qn::Offline);
        if (peerType != vms::api::PeerType::server)
        {
            // This server hasn't own DB
            for(const QnResourcePtr& camera: resourcePool()->getAllCameras(res))
                camera->setStatus(Qn::Offline);
        }
    }
    m_delayedOnlineStatus.remove(peer);
}

void QnServerMessageProcessor::onResourceStatusChanged(
    const QnResourcePtr& resource,
    Qn::ResourceStatus status,
    ec2::NotificationSource /*source*/)
{
    if (resource->getId() == commonModule()->moduleGUID() && status != Qn::Online)
    {
        // It's own server. change status to online.
        auto connection = commonModule()->ec2Connection();
        if (connection)
        {
            auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);
            resourceManager->setResourceStatusSync(resource->getId(), Qn::Online);
            resource->setStatus(Qn::Online, Qn::StatusChangeReason::GotFromRemotePeer);
        }
    }
    else if (resource->getParentId() == commonModule()->moduleGUID() &&
        resource.dynamicCast<QnStorageResource>())
    {
        NX_VERBOSE(this, lit("%1 Received statusChanged signal for storage %2 from remote peer. This storage is our own resource. Ignoring.")
            .arg(QString::fromLatin1(Q_FUNC_INFO))
            .arg(resource->getId().toString()));

        // Rewrite resource status to DB and send to the foreign peer
        if (resource->getStatus() != status)
            m_serverModule->statusWatcher()->updateResourceStatus(resource);
    }
    else
    {
        resource->setStatus(status, Qn::StatusChangeReason::GotFromRemotePeer);
    }
}

bool QnServerMessageProcessor::isLocalAddress(const QString& addr) const
{
    if (addr == "localhost" || addr == "127.0.0.1")
        return true;
    if( !m_mServer )
        m_mServer = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    if (m_mServer)
    {
        nx::network::HostAddress hostAddr(addr);
        for(const auto& serverAddr: m_mServer->getNetAddrList())
        {
            if (hostAddr == serverAddr.address)
                return true;
        }
    }
    return false;
}

void QnServerMessageProcessor::execBusinessActionInternal(
    const nx::vms::event::AbstractActionPtr& action)
{
    m_serverModule->eventMessageBus()->at_actionReceived(action);
}

void QnServerMessageProcessor::at_updateChunkReceived(
    const QString& updateId, const QByteArray& data, qint64 offset)
{
    m_serverModule->serverUpdateTool()->addUpdateFileChunkAsync(updateId, data, offset);
}

void QnServerMessageProcessor::at_updateInstallationRequested(const QString& updateId)
{
    m_serverModule->serverUpdateTool()->installUpdate(
        updateId, QnServerUpdateTool::UpdateType::Delayed);
}

void QnServerMessageProcessor::at_remotePeerUnauthorized(const QnUuid& id)
{
    QnResourcePtr mServer = resourcePool()->getResourceById(id);
    if (mServer)
        mServer->setStatus(Qn::Unauthorized);
}

bool QnServerMessageProcessor::canRemoveResource(const QnUuid& resourceId)
{
    QnResourcePtr res = resourcePool()->getResourceById(resourceId);
    bool isOwnServer = (res && res->getId() == commonModule()->moduleGUID());
    if (isOwnServer)
        return false;

    QnStorageResourcePtr storage = res.dynamicCast<QnStorageResource>();
    bool isOwnStorage = (storage && storage->getParentId() == commonModule()->moduleGUID());
    if (!isOwnStorage)
        return true;

    return (storage->isExternal() || !storage->isWritable());
}

void QnServerMessageProcessor::removeResourceIgnored(const QnUuid& resourceId)
{
    QnMediaServerResourcePtr mServer = resourcePool()->getResourceById<QnMediaServerResource>(resourceId);
    QnStorageResourcePtr storage = resourcePool()->getResourceById<QnStorageResource>(resourceId);
    bool isOwnServer = (mServer && mServer->getId() == commonModule()->moduleGUID());
    bool isOwnStorage = (storage && storage->getParentId() == commonModule()->moduleGUID());
    if (isOwnServer)
    {
        vms::api::MediaServerData apiServer;
        ec2::fromResourceToApi(mServer, apiServer);
        auto connection = commonModule()->ec2Connection();
        connection->getMediaServerManager(Qn::kSystemAccess)->save(
            apiServer,
            ec2::DummyHandler::instance(),
            &ec2::DummyHandler::onRequestDone);
    }
    else if (isOwnStorage && !storage->isExternal() && storage->isWritable())
    {
        vms::api::StorageDataList apiStorages;
        ec2::fromResourceListToApi(QnStorageResourceList() << storage, apiStorages);
        commonModule()->ec2Connection()->getMediaServerManager(Qn::kSystemAccess)->saveStorages(
            apiStorages,
            ec2::DummyHandler::instance(),
            &ec2::DummyHandler::onRequestDone);
    }
}

QnResourceFactory* QnServerMessageProcessor::getResourceFactory() const
{
    return commonModule()->resourceDiscoveryManager();
}
