#include "server_message_processor.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <media_server/server_update_tool.h>

#include "serverutil.h"
#include "transaction/transaction_message_bus.h"
#include "business/business_message_bus.h"
#include "settings.h"
#include "api/app_server_connection.h"


QnServerMessageProcessor::QnServerMessageProcessor()
:
    base_type(),
    m_serverPort( MSSettings::roSettings()->value(nx_ms_conf::RTSP_PORT, nx_ms_conf::DEFAULT_RTSP_PORT).toInt() )
{

}

void QnServerMessageProcessor::updateAllIPList(const QnId& id, const QList<QHostAddress>& addrList)
{
    QStringList addrListStr;
    foreach(const QHostAddress& addr, addrList)
        addrListStr << addr.toString();

    updateAllIPList(id, addrListStr);
}

void QnServerMessageProcessor::updateAllIPList(const QnId& id, const QString& addr)
{
    updateAllIPList(id, QList<QString>() << addr);
}

void QnServerMessageProcessor::updateAllIPList(const QnId& id, const QList<QString>& addrList)
{
    QMutexLocker lock(&m_mutexAddrList);

    QHash<QnId, QList<QString> >::iterator itr = m_addrById.find(id);
    if (itr != m_addrById.end()) 
    {
        foreach(const QString& addr, itr.value()) 
        {
            QHash<QString, int>::iterator itrAddr = m_allIPAddress.find(addr);
            if (itrAddr != m_allIPAddress.end()) {
                if (--itrAddr.value() < 1)
                    m_allIPAddress.erase(itrAddr);
            }
        }
    }
    else {
        itr = m_addrById.insert(id, QList<QString>());
    }

    *itr = addrList;

    foreach(const QString& addr, addrList) 
    {
        QHash<QString, int>::iterator itrAddr = m_allIPAddress.find(addr);
        if (itrAddr != m_allIPAddress.end())
            ++itrAddr.value();
        else
            m_allIPAddress.insert(addr, 1);
    }

}

void QnServerMessageProcessor::removeIPList(const QnId& id)
{
    QMutexLocker lock(&m_mutexAddrList);

    QHash<QnId, QList<QString> >::iterator itr = m_addrById.find(id);
    if (itr != m_addrById.end()) 
    {
        foreach(const QString& addr, itr.value()) 
        {
            QHash<QString, int>::iterator itrAddr = m_allIPAddress.find(addr);
            if (itrAddr != m_allIPAddress.end()) {
                if (--itrAddr.value() < 1)
                    m_allIPAddress.erase(itrAddr);
            }
        }
        m_addrById.erase(itr);
    }
}

void QnServerMessageProcessor::updateResource(const QnResourcePtr &resource) {
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
        bool needProxyToCamera = true;
        if (resource->getParentId() != ownMediaServer->getId()) {
            resource->addFlags( QnResource::foreigner );
        }
        else {
#ifdef EDGE_SERVER
            needProxyToCamera = false;
#endif
        }
        // update all known IP list
        if (needProxyToCamera) {
            const QnVirtualCameraResource* camRes = dynamic_cast<const QnVirtualCameraResource*>(resource.data());
            updateAllIPList(camRes->getId(), camRes->getHostAddress());
        }
    }

    if (isServer) 
    {
        if (resource->getId() != ownMediaServer->getId()) {
            resource->addFlags( QnResource::foreigner );
            // update all known IP list
            updateAllIPList(resource->getId(), dynamic_cast<const QnMediaServerResource*>(resource.data())->getNetAddrList());
        }
    }

    // We are always online
    if (isServer && resource->getId() == serverGuid()) 
    {
        if (resource->getStatus() != QnResource::Online && resource->getStatus() != QnResource::NotDefined) {
            qWarning() << "ServerMessageProcessor: Received message that our status is " << resource->getStatus() << ". change to online";
            resource->setStatus(QnResource::Online);
        }
    }

    if (QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId()))
        ownResource->update(resource);
    else
        qnResPool->addResource(resource);

    if (isServer && resource->getId() == serverGuid())
        syncStoragesToSettings(ownMediaServer);

}

void QnServerMessageProcessor::afterRemovingResource(const QnId& id)
{
    QnCommonMessageProcessor::afterRemovingResource(id);
    removeIPList(id);
}

void QnServerMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnServerMessageProcessor::at_remotePeerFound );
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnServerMessageProcessor::at_remotePeerLost );

    connect( connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateChunkReceived,
        this, &QnServerMessageProcessor::at_updateChunkReceived );
    connect( connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateInstallationRequested,
        this, &QnServerMessageProcessor::at_updateInstallationRequested );

    QnCommonMessageProcessor::init(connection);
}

bool QnServerMessageProcessor::isKnownAddr(const QString& addr) const
{
    QMutexLocker lock(&m_mutexAddrList);
    //return true;
    return !addr.isEmpty() && m_allIPAddress.contains(addr);
}

/*
* EC2 related processing. Need move to other class
*/

void QnServerMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data, bool /*isProxy*/)
{
    QnResourcePtr res = qnResPool->getResourceById(data.peer.id);
    if (res)
        res->setStatus(QnResource::Online);

}

void QnServerMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData data, bool /*isProxy*/)
{
    QnResourcePtr res = qnResPool->getResourceById(data.peer.id);
    if (res) {
        res->setStatus(QnResource::Offline);
        if (data.peer.peerType != Qn::PT_Server) {
            // This server hasn't own DB
            foreach(QnResourcePtr camera, qnResPool->getAllCameras(res))
                camera->setStatus(QnResource::Offline);
        }
    }
}

void QnServerMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) 
{
    if (resource->getId() == qnCommon->moduleGUID() && resource->getStatus() != QnResource::Online)
    {
        // it's own server. change status to online
        QnAppServerConnectionFactory::getConnection2()->getResourceManager()->setResourceStatusSync(resource->getId(), QnResource::Online);
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
    const nx_http::BufferType& desiredServerGuid = nx_http::getHeaderValue( request.headers, "x-server-guid" );
    nx_http::HttpHeaders::const_iterator xServerGuidIter = request.headers.find( "x-server-guid" );
    if( xServerGuidIter != request.headers.end() )
    {
        const QByteArray& localServerGUID = qnCommon->moduleGUID().toByteArray();
        return desiredServerGuid != localServerGUID;
    }

    const QString& urlHost = request.requestLine.url.host();
    const int port = request.requestLine.url.port( nx_http::DEFAULT_HTTP_PORT );
    const bool _isLocalAddress = isLocalAddress(urlHost);
    if (_isLocalAddress)
        return port != m_serverPort; //if false, request addressed to us. If true, proxying to some local service

    return isKnownAddr(urlHost); // is it camera or other server address?
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

bool QnServerMessageProcessor::canRemoveResource(const QnId& resourceId) 
{ 
    QnResourcePtr res = qnResPool->getResourceById(resourceId);
    bool isOwnServer = (res && res->getId() == qnCommon->moduleGUID());
    return !isOwnServer;
}

void QnServerMessageProcessor::removeResourceIgnored(const QnId& resourceId) 
{
    QnMediaServerResourcePtr mServer = qnResPool->getResourceById(resourceId).dynamicCast<QnMediaServerResource>();
    bool isOwnServer = (mServer && mServer->getId() == qnCommon->moduleGUID());
    if (isOwnServer) {
        QnMediaServerResourcePtr savedServer;
        QnAppServerConnectionFactory::getConnection2()->getMediaServerManager()->saveSync(mServer, &savedServer);
        QnAppServerConnectionFactory::getConnection2()->getResourceManager()->setResourceStatusSync(mServer->getId(), QnResource::Online);
    }
}
