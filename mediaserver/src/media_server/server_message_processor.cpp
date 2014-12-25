#include "server_message_processor.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <media_server/server_update_tool.h>
#include <media_server/settings.h>
#include <nx_ec/dummy_handler.h>
#include <utils/common/log.h>

#include "serverutil.h"
#include "transaction/transaction_message_bus.h"
#include "business/business_message_bus.h"
#include "settings.h"
#include "nx_ec/data/api_connection_data.h"
#include "api/app_server_connection.h"
#include "utils/network/router.h"

#include <utils/common/app_info.h>
#include "core/resource/storage_resource.h"

QnServerMessageProcessor::QnServerMessageProcessor()
:
    base_type(),
    m_serverPort( MSSettings::roSettings()->value(nx_ms_conf::SERVER_PORT, nx_ms_conf::DEFAULT_SERVER_PORT).toInt() )
{
}

void QnServerMessageProcessor::updateResource(const QnResourcePtr &resource) 
{
    QnCommonMessageProcessor::updateResource(resource);
    QnMediaServerResourcePtr ownMediaServer = qnResPool->getResourceById(serverGuid()).dynamicCast<QnMediaServerResource>();

    const bool isServer = dynamic_cast<const QnMediaServerResource*>(resource.data()) != nullptr;
    const bool isCamera = dynamic_cast<const QnVirtualCameraResource*>(resource.data()) != nullptr;
    const bool isUser = dynamic_cast<const QnUserResource*>(resource.data()) != nullptr;
    const bool isVideowall = dynamic_cast<const QnVideoWallResource*>(resource.data()) != nullptr;
    const bool isStorage = dynamic_cast<const QnAbstractStorageResource*>(resource.data()) != nullptr;

    if (!isServer && !isCamera && !isUser && !isVideowall && !isStorage)
        return;

    //storing all servers' cameras too
    // If camera from other server - marking it
    
    if (isCamera) 
    {
        if (resource->getParentId() != ownMediaServer->getId())
            resource->addFlags( Qn::foreigner );
        else {
#if 0
            QnResourceTypePtr thirdPartyType = qnResTypePool->getResourceTypeByName("THIRD_PARTY Camera");
            QnResourceTypePtr desktopCameraType = qnResTypePool->getResourceTypeByName("SERVER_DESKTOP_CAMERA");
            if (thirdPartyType && desktopCameraType && 
                resource->getTypeId() != desktopCameraType->getId() && resource->getTypeId() != thirdPartyType->getId()) 
            {
                resource->setTypeId(thirdPartyType->getId());
                QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
                QnVirtualCameraResourceList cameras;
                cameras << camera;
                m_connection->getCameraManager()->save(cameras, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
            }
#endif
        }
    }

    if (isServer) 
    {
        if (resource->getId() != ownMediaServer->getId())
            resource->addFlags( Qn::foreigner );
    }

    // We are always online
    if (isServer && resource->getId() == serverGuid()) 
    {
        if (resource->getStatus() != Qn::Online && resource->getStatus() != Qn::NotDefined) {
            qWarning() << "ServerMessageProcessor: Received message that our status is " << resource->getStatus() << ". change to online";
            resource->setStatus(Qn::Online);
        }
    }
    QnUuid resId = resource->getId();
    if (QnResourcePtr ownResource = qnResPool->getResourceById(resId))
        ownResource->update(resource);
    else
        qnResPool->addResource(resource);

    if (m_delayedOnlineStatus.contains(resId)) {
        m_delayedOnlineStatus.remove(resId);
        resource->setStatus(Qn::Online);
    }
}

void QnServerMessageProcessor::afterRemovingResource(const QnUuid& id)
{
    QnCommonMessageProcessor::afterRemovingResource(id);
}

void QnServerMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection) {
    QnCommonMessageProcessor::init(connection);
}

void QnServerMessageProcessor::connectToConnection(const ec2::AbstractECConnectionPtr &connection) {
    base_type::connectToConnection(connection);

    connect(connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateChunkReceived,
        this, &QnServerMessageProcessor::at_updateChunkReceived);
    connect(connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateInstallationRequested,
        this, &QnServerMessageProcessor::at_updateInstallationRequested);

    connect( connection, &ec2::AbstractECConnection::remotePeerUnauthorized, this, &QnServerMessageProcessor::at_remotePeerUnauthorized );

    connect(connection->getMiscManager().get(), &ec2::AbstractMiscManager::systemNameChangeRequested,
        this, [this](const QString &systemName) { changeSystemName(systemName); });
}

void QnServerMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) {
    base_type::disconnectFromConnection(connection);
    connection->getUpdatesManager()->disconnect(this);
    connection->getMiscManager()->disconnect(this);
}

void QnServerMessageProcessor::handleRemotePeerFound(const ec2::ApiPeerAliveData &data) {
    base_type::handleRemotePeerFound(data);
    QnResourcePtr res = qnResPool->getResourceById(data.peer.id);
    if (res)
        res->setStatus(Qn::Online);
    else
        m_delayedOnlineStatus << data.peer.id;
}

void QnServerMessageProcessor::handleRemotePeerLost(const ec2::ApiPeerAliveData &data) {
    base_type::handleRemotePeerLost(data);
    QnResourcePtr res = qnResPool->getResourceById(data.peer.id);
    if (res) {
        res->setStatus(Qn::Offline);
        if (data.peer.peerType != Qn::PT_Server) {
            // This server hasn't own DB
            for(const QnResourcePtr& camera: qnResPool->getAllCameras(res))
                camera->setStatus(Qn::Offline);
        }
    }
    m_delayedOnlineStatus.remove(data.peer.id);
}

void QnServerMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) 
{
    if (resource->getId() == qnCommon->moduleGUID() && status != Qn::Online)
    {
        // it's own server. change status to online
        QnAppServerConnectionFactory::getConnection2()->getResourceManager()->setResourceStatusLocalSync(resource->getId(), Qn::Online);
        resource->setStatus(Qn::Online, true);
    }
    else {
        resource->setStatus(status, true);
    }
}

bool QnServerMessageProcessor::isLocalAddress(const QString& addr) const
{
    if (addr == "localhost" || addr == "127.0.0.1")
        return true;
    if( !m_mServer )
        m_mServer = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (m_mServer) 
    {
        QHostAddress hostAddr(addr);
        for(const QHostAddress& serverAddr: m_mServer->getNetAddrList())
        {
            if (hostAddr == serverAddr)
                return true;
        }
    }
    return false;
}

bool QnServerMessageProcessor::isProxy(const nx_http::Request& request) const
{
    nx_http::HttpHeaders::const_iterator xServerGuidIter = request.headers.find( "x-server-guid" );
    if( xServerGuidIter != request.headers.end() )
    {
        const nx_http::BufferType& desiredServerGuid = xServerGuidIter->second;
        const QByteArray localServerGUID = qnCommon->moduleGUID().toByteArray();
        return desiredServerGuid != localServerGUID;
    }

    nx_http::BufferType desiredCameraGuid;
    nx_http::HttpHeaders::const_iterator xCameraGuidIter = request.headers.find( "x-camera-guid" );
    if( xCameraGuidIter != request.headers.end() )
    {
        desiredCameraGuid = xCameraGuidIter->second;
    }
    else {
        desiredCameraGuid = request.getCookieValue("x-camera-guid");
    }
    if (!desiredCameraGuid.isEmpty()) {
        QnResourcePtr camera = qnResPool->getResourceById(desiredCameraGuid);
        return camera != 0;
    }

    return false;
}

void QnServerMessageProcessor::execBusinessActionInternal(const QnAbstractBusinessActionPtr& action) {
    qnBusinessMessageBus->at_actionReceived(action);
}

void QnServerMessageProcessor::at_updateChunkReceived(const QString &updateId, const QByteArray &data, qint64 offset) {
    QnServerUpdateTool::instance()->addUpdateFileChunkAsync(updateId, data, offset);
}

void QnServerMessageProcessor::at_updateInstallationRequested(const QString &updateId) {
    QnServerUpdateTool::instance()->installUpdate(updateId);
}

void QnServerMessageProcessor::at_remotePeerUnauthorized(const QnUuid& id)
{
    QnResourcePtr mServer = qnResPool->getResourceById(id);
    if (mServer)
        mServer->setStatus(Qn::Unauthorized);
}

bool QnServerMessageProcessor::canRemoveResource(const QnUuid& resourceId) 
{ 
    QnResourcePtr res = qnResPool->getResourceById(resourceId);
    bool isOwnServer = (res && res->getId() == qnCommon->moduleGUID());
    if (isOwnServer)
        return false;
    QnStorageResourcePtr storage = res.dynamicCast<QnStorageResource>();
    bool isOwnStorage = (storage && storage->getParentId() == qnCommon->moduleGUID());
    if (isOwnStorage && !storage->isExternal())
        return false;

    return true;
}

void QnServerMessageProcessor::removeResourceIgnored(const QnUuid& resourceId) 
{
    QnMediaServerResourcePtr mServer = qnResPool->getResourceById(resourceId).dynamicCast<QnMediaServerResource>();
    QnStorageResourcePtr storage = qnResPool->getResourceById(resourceId).dynamicCast<QnStorageResource>();
    bool isOwnServer = (mServer && mServer->getId() == qnCommon->moduleGUID());
    bool isOwnStorage = (storage && storage->getParentId() == qnCommon->moduleGUID());
    if (isOwnServer) {
        QnMediaServerResourcePtr savedServer;
        QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->saveSync(mServer, &savedServer);
        QnAppServerConnectionFactory::getConnection2()->getResourceManager()->setResourceStatusLocalSync(mServer->getId(), Qn::Online);
    }
    else if (isOwnStorage && !storage->isExternal()) {
        QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->saveStoragesSync(QnAbstractStorageResourceList() << storage);
    }
}
