#include "server_message_processor.h"

#include "core/resource_management/resource_pool.h"
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <media_server/server_update_tool.h>

#include "serverutil.h"
#include "transaction/transaction_message_bus.h"
#include "business/business_message_bus.h"

#include "version.h"


QnServerMessageProcessor::QnServerMessageProcessor():
        base_type()
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

    bool isServer = resource.dynamicCast<QnMediaServerResource>();
    bool isCamera = resource.dynamicCast<QnVirtualCameraResource>();
    bool isUser = resource.dynamicCast<QnUserResource>();
    bool isVideowall = resource.dynamicCast<QnVideoWallResource>();

    if (!isServer && !isCamera && !isUser && !isVideowall)
        return;

    //storing all servers' cameras too
    // If camera from other server - marking it
    
    if (isCamera) 
    {
        if (resource->getParentId() != ownMediaServer->getId())
            resource->addFlags( QnResource::foreigner );
        // update all known IP list
        QnVirtualCameraResourcePtr camRes = resource.dynamicCast<QnVirtualCameraResource>();
        updateAllIPList(camRes->getId(), camRes->getHostAddress());
    }

    if (isServer) 
    {
        if (resource->getId() != ownMediaServer->getId()) {
            resource->addFlags( QnResource::foreigner );
            // update all known IP list
            updateAllIPList(resource->getId(), resource.dynamicCast<QnMediaServerResource>()->getNetAddrList());
        }
    }

    bool needUpdateServer = false;
    // We are always online
    if (isServer && resource->getId() == serverGuid()) {
        if (resource->getStatus() != QnResource::Online) {
            qWarning() << "XYZ1: Received message that our status is " << resource->getStatus();
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

void QnServerMessageProcessor::init(ec2::AbstractECConnectionPtr connection)
{
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnServerMessageProcessor::at_remotePeerFound );
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnServerMessageProcessor::at_remotePeerLost );

    connect(connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateChunkReceived,
            this, &QnServerMessageProcessor::at_updateChunkReceived);
    connect(connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateInstallationRequested,
            this, &QnServerMessageProcessor::at_updateInstallationRequested);
    connect(connection->getUpdatesManager().get(), &ec2::AbstractUpdatesManager::updateRequested,
            this, &QnServerMessageProcessor::at_updateRequested);

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

void QnServerMessageProcessor::at_remotePeerFound(ec2::ApiServerAliveData data, bool isProxy)
{
    Q_UNUSED(isProxy)

    QnResourcePtr res = qnResPool->getResourceById(data.serverId);
    if (res)
        res->setStatus(QnResource::Online);

}

void QnServerMessageProcessor::at_remotePeerLost(ec2::ApiServerAliveData data, bool isProxy)
{
    Q_UNUSED(isProxy)

    QnResourcePtr res = qnResPool->getResourceById(data.serverId);
    if (res) {
        res->setStatus(QnResource::Offline);
        if (data.isClient) {
            // This media server hasn't own DB
            foreach(QnResourcePtr camera, qnResPool->getAllCameras(res))
                camera->setStatus(QnResource::Offline);
        }
    }
}

void QnServerMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) {
    resource->setStatus(status, true);
}

bool QnServerMessageProcessor::isProxy(void* opaque, const QUrl& url) {
    return static_cast<QnServerMessageProcessor*> (opaque)->isProxy(url);
}

bool QnServerMessageProcessor::isProxy(const QUrl& url) {
    return isKnownAddr(url.host());
}

void QnServerMessageProcessor::execBusinessActionInternal(QnAbstractBusinessActionPtr action) {
    qnBusinessMessageBus->at_actionReceived(action);
}

void QnServerMessageProcessor::at_updateChunkReceived(const QString &updateId, const QByteArray &data, qint64 offset) {
    QnServerUpdateTool::instance()->addUpdateFileChunk(updateId, data, offset);
}

void QnServerMessageProcessor::at_updateInstallationRequested(const QString &updateId) {
    QnServerUpdateTool::instance()->installUpdate(updateId);
}

void QnServerMessageProcessor::at_updateRequested(const QString &updateId, const QByteArray &data, const QString &targetId) {
    Q_UNUSED(updateId)
    QnServerUpdateTool::instance()->uploadAndInstallUpdate(targetId, data);
}
