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
#include "transaction/transaction_message_bus.h"
#include "business/business_message_bus.h"
#include "settings.h"
#include "nx_ec/data/api_conversion_functions.h"
#include "nx_ec/data/api_connection_data.h"
#include <nx_ec/managers/abstract_server_manager.h>
#include "api/app_server_connection.h"
#include "network/router.h"
#include <network/module_finder.h>

#include <utils/common/app_info.h>
#include "core/resource/storage_resource.h"
#include "http/custom_headers.h"
#include "resource_status_watcher.h"

QnServerMessageProcessor::QnServerMessageProcessor()
:
    base_type(),
    m_serverPort( MSSettings::roSettings()->value(nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT).toInt() )
{
}

void QnServerMessageProcessor::updateResource(const QnResourcePtr &resource, ec2::NotificationSource source)
{
    QnCommonMessageProcessor::updateResource(resource, source);
    QnMediaServerResourcePtr ownMediaServer = resourcePool()->getResourceById<QnMediaServerResource>(serverGuid());

    if (resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (resource->getParentId() != ownMediaServer->getId())
            resource->addFlags( Qn::foreigner );
    }

    if (resource.dynamicCast<QnMediaServerResource>())
    {
        if (resource->getId() == ownMediaServer->getId())
        {
            ec2::ApiMediaServerData ownData;

            ec2::ApiMediaServerData newData;

            ec2::fromResourceToApi(ownMediaServer, ownData);
            ec2::fromResourceToApi(resource.staticCast<QnMediaServerResource>(), newData);

            if (ownData != newData && source == ec2::NotificationSource::Remote)
            {
                // if remote peer send update for our server then ignore it and resend our own data
                commonModule()->ec2Connection()->getMediaServerManager(Qn::kSystemAccess)->saveSync(ownData);
                return;
            }

            // We are always online
            if (resource->getStatus() != Qn::Online && resource->getStatus() != Qn::NotDefined)
            {
                qWarning() << "ServerMessageProcessor: Received message that our status is " << resource->getStatus() << ". change to online";
                resource->setStatus(Qn::Online);
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

void QnServerMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection) {
    QnCommonMessageProcessor::init(connection);
}

void QnServerMessageProcessor::startReceivingLocalNotifications(const ec2::AbstractECConnectionPtr &connection)
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

void QnServerMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection) {
    base_type::connectToConnection(connection);

    connect(connection->getUpdatesNotificationManager().get(), &ec2::AbstractUpdatesNotificationManager::updateChunkReceived,
        this, &QnServerMessageProcessor::at_updateChunkReceived);
    connect(connection->getUpdatesNotificationManager().get(), &ec2::AbstractUpdatesNotificationManager::updateInstallationRequested,
        this, &QnServerMessageProcessor::at_updateInstallationRequested);

    connect(connection, &ec2::AbstractECConnection::remotePeerUnauthorized,
        this, &QnServerMessageProcessor::at_remotePeerUnauthorized);
    connect(connection, &ec2::AbstractECConnection::reverseConnectionRequested,
        this, &QnServerMessageProcessor::at_reverseConnectionRequested);

    connect(connection->getMiscNotificationManager().get(), &ec2::AbstractMiscNotificationManager::systemIdChangeRequested,
            this, [this](const QnUuid& systemId, qint64 sysIdTime, ec2::Timestamp tranLogTime)
                  {
                      ConfigureSystemData configSystemData;
                      configSystemData.localSystemId = systemId;
                      configSystemData.sysIdTime = sysIdTime;
                      configSystemData.tranLogTime = tranLogTime;
                      configSystemData.wholeSystem = true;
                      if (m_connection)
                        changeLocalSystemId(configSystemData, m_connection->messageBus());
                  });
}

void QnServerMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) {
    base_type::disconnectFromConnection(connection);
    connection->getUpdatesNotificationManager()->disconnect(this);
    connection->getMiscNotificationManager()->disconnect(this);
}

void QnServerMessageProcessor::handleRemotePeerFound(const ec2::ApiPeerAliveData &data) {
    base_type::handleRemotePeerFound(data);
    QnResourcePtr res = resourcePool()->getResourceById(data.peer.id);
    if (res)
        res->setStatus(Qn::Online);
    else
        m_delayedOnlineStatus << data.peer.id;

    if (QnModuleFinder *moduleFinder = QnModuleFinder::instance())
        moduleFinder->setModuleStatus(data.peer.id, Qn::Online);
}

void QnServerMessageProcessor::handleRemotePeerLost(const ec2::ApiPeerAliveData &data) {
    base_type::handleRemotePeerLost(data);
    QnResourcePtr res = resourcePool()->getResourceById(data.peer.id);
    if (res) {
        res->setStatus(Qn::Offline);
        if (data.peer.peerType != Qn::PT_Server) {
            // This server hasn't own DB
            for(const QnResourcePtr& camera: resourcePool()->getAllCameras(res))
                camera->setStatus(Qn::Offline);
        }
    }
    m_delayedOnlineStatus.remove(data.peer.id);

    if (QnModuleFinder *moduleFinder = QnModuleFinder::instance())
        moduleFinder->setModuleStatus(data.peer.id, Qn::Offline);
}

void QnServerMessageProcessor::onResourceStatusChanged(
    const QnResourcePtr &resource,
    Qn::ResourceStatus status,
    ec2::NotificationSource source)
{
    if (resource->getId() == qnCommon->moduleGUID() && status != Qn::Online)
    {
        // it's own server. change status to online
        // it's own server. change status to online
        auto connection = commonModule()->ec2Connection();
        auto manager = connection->getResourceManager(Qn::kSystemAccess);
        manager->setResourceStatusSync(resource->getId(), Qn::Online);
        resource->setStatus(Qn::Online, Qn::StatusChangeReason::GotFromRemotePeer);
    }
    else if (resource->getParentId() == qnCommon->moduleGUID() &&
        resource.dynamicCast<QnStorageResource>())
    {
        NX_LOG(lit("%1 Received statusChanged signal for storage %2 from remote peer. This storage is our own resource. Ignoring.")
            .arg(QString::fromLatin1(Q_FUNC_INFO))
            .arg(resource->getId().toString()), cl_logDEBUG2);

        // Rewrite resource status to DB and send to the foreign peer
        if (resource->getStatus() != status)
            QnResourceStatusWatcher::instance()->updateResourceStatus(resource);
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
        m_mServer = resourcePool()->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (m_mServer)
    {
        HostAddress hostAddr(addr);
        for(const auto& serverAddr: m_mServer->getNetAddrList())
        {
            if (hostAddr == serverAddr.address)
                return true;
        }
    }
    return false;
}

void QnServerMessageProcessor::registerProxySender(QnUniversalTcpListener* tcpListener) {
    m_universalTcpListener = tcpListener;
}

void QnServerMessageProcessor::execBusinessActionInternal(const QnAbstractBusinessActionPtr& action) {
    qnBusinessMessageBus->at_actionReceived(action);
}

void QnServerMessageProcessor::at_updateChunkReceived(const QString &updateId, const QByteArray &data, qint64 offset) {
    QnServerUpdateTool::instance()->addUpdateFileChunkAsync(updateId, data, offset);
}

void QnServerMessageProcessor::at_updateInstallationRequested(const QString &updateId) {
    QnServerUpdateTool::instance()->installUpdate(
        updateId, QnServerUpdateTool::UpdateType::Delayed);
}

void QnServerMessageProcessor::at_reverseConnectionRequested(const ec2::ApiReverseConnectionData &data) {
    if (m_universalTcpListener) {
        QnRoute route = QnRouter::instance()->routeTo(data.targetServer);

        // just to be sure that we have direct access to the server
        if (route.gatewayId.isNull() && !route.addr.isNull())
            m_universalTcpListener->addProxySenderConnections(route.addr, data.socketCount);
    }
}

void QnServerMessageProcessor::at_remotePeerUnauthorized(const QnUuid& id)
{
    QnResourcePtr mServer = resourcePool()->getResourceById(id);
    if (mServer)
        mServer->setStatus(Qn::Unauthorized);

    if (QnModuleFinder *moduleFinder = QnModuleFinder::instance())
        moduleFinder->setModuleStatus(id, Qn::Unauthorized);
}

bool QnServerMessageProcessor::canRemoveResource(const QnUuid& resourceId)
{
    QnResourcePtr res = resourcePool()->getResourceById(resourceId);
    bool isOwnServer = (res && res->getId() == qnCommon->moduleGUID());
    if (isOwnServer)
        return false;

    QnStorageResourcePtr storage = res.dynamicCast<QnStorageResource>();
    bool isOwnStorage = (storage && storage->getParentId() == qnCommon->moduleGUID());
    if (!isOwnStorage)
        return true;

    return (storage->isExternal() || !storage->isWritable());
}

void QnServerMessageProcessor::removeResourceIgnored(const QnUuid& resourceId)
{
    QnMediaServerResourcePtr mServer = resourcePool()->getResourceById<QnMediaServerResource>(resourceId);
    QnStorageResourcePtr storage = resourcePool()->getResourceById<QnStorageResource>(resourceId);
    bool isOwnServer = (mServer && mServer->getId() == qnCommon->moduleGUID());
    bool isOwnStorage = (storage && storage->getParentId() == qnCommon->moduleGUID());
    if (isOwnServer)
    {
        ec2::ApiMediaServerData apiServer;
        ec2::fromResourceToApi(mServer, apiServer);
        auto connection = commonModule()->ec2Connection();
        connection->getMediaServerManager(Qn::kSystemAccess)->saveSync(apiServer);
        connection->getResourceManager(Qn::kSystemAccess)->setResourceStatusSync(apiServer.id, Qn::Online);
    }
    else if (isOwnStorage && !storage->isExternal() && storage->isWritable())
    {
        ec2::ApiStorageDataList apiStorages;
        fromResourceListToApi(QnStorageResourceList() << storage, apiStorages);
        commonModule()->ec2Connection()->getMediaServerManager(Qn::kSystemAccess)->saveStoragesSync(apiStorages);
    }
}

QnResourceFactory* QnServerMessageProcessor::getResourceFactory() const
{
    return QnResourceDiscoveryManager::instance();
}
