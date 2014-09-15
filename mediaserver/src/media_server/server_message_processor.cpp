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

#include "version.h"

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

    if (!isServer && !isCamera && !isUser && !isVideowall)
        return;

    //storing all servers' cameras too
    // If camera from other server - marking it
    
    if (isCamera) 
    {
        if (resource->getParentId() != ownMediaServer->getId())
            resource->addFlags( Qn::foreigner );
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

    if (QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId()))
        ownResource->update(resource);
    else
        qnResPool->addResource(resource);

}

void QnServerMessageProcessor::afterRemovingResource(const QUuid& id)
{
    QnCommonMessageProcessor::afterRemovingResource(id);
}

void QnServerMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    connect(connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateChunkReceived,
            this, &QnServerMessageProcessor::at_updateChunkReceived);
    connect(connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateInstallationRequested,
            this, &QnServerMessageProcessor::at_updateInstallationRequested);
    connect(connection->getMiscManager().get(), &ec2::AbstractMiscManager::systemNameChangeRequested,
            this, &QnServerMessageProcessor::at_systemNameChangeRequested);

    connect( connection, &ec2::AbstractECConnection::remotePeerUnauthorized, this, &QnServerMessageProcessor::at_remotePeerUnauthorized );

    QnCommonMessageProcessor::init(connection);
}

void QnServerMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) 
{
    if (resource->getId() == qnCommon->moduleGUID() && status != Qn::Online)
    {
        // it's own server. change status to online
        QnAppServerConnectionFactory::getConnection2()->getResourceManager()->setResourceStatusSync(resource->getId(), Qn::Online);
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
        foreach(const QHostAddress& serverAddr, m_mServer->getNetAddrList())
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
        const QByteArray& localServerGUID = qnCommon->moduleGUID().toByteArray();
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
    QnServerUpdateTool::instance()->addUpdateFileChunk(updateId, data, offset);
}

void QnServerMessageProcessor::at_updateInstallationRequested(const QString &updateId) {
    QnServerUpdateTool::instance()->installUpdate(updateId);
}

void QnServerMessageProcessor::at_systemNameChangeRequested(const QString &systemName) {
    if (qnCommon->localSystemName() == systemName)
        return;

    qnCommon->setLocalSystemName(systemName);
    QnMediaServerResourcePtr server = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (!server) {
        NX_LOG("Cannot find self server resource!", cl_logERROR);
        return;

        MSSettings::roSettings()->setValue("systemName", systemName);
        server->setSystemName(systemName);
        m_connection->getMediaServerManager()->save(server, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }
}

void QnServerMessageProcessor::at_remotePeerUnauthorized(const QUuid& id)
{
    QnResourcePtr mServer = qnResPool->getResourceById(id);
    if (mServer)
        mServer->setStatus(Qn::Unauthorized);
}

bool QnServerMessageProcessor::canRemoveResource(const QUuid& resourceId) 
{ 
    QnResourcePtr res = qnResPool->getResourceById(resourceId);
    bool isOwnServer = (res && res->getId() == qnCommon->moduleGUID());
    return !isOwnServer;
}

void QnServerMessageProcessor::removeResourceIgnored(const QUuid& resourceId) 
{
    QnMediaServerResourcePtr mServer = qnResPool->getResourceById(resourceId).dynamicCast<QnMediaServerResource>();
    bool isOwnServer = (mServer && mServer->getId() == qnCommon->moduleGUID());
    if (isOwnServer) {
        QnMediaServerResourcePtr savedServer;
        QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->saveSync(mServer, &savedServer);
        QnAppServerConnectionFactory::getConnection2()->getResourceManager()->setResourceStatusSync(mServer->getId(), Qn::Online);
    }
}
