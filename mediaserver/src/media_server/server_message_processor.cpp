#include "server_message_processor.h"
#include "core/resource_management/resource_pool.h"
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include "serverutil.h"
#include "transaction/transaction_message_bus.h"


QnServerMessageProcessor::QnServerMessageProcessor():
        base_type()
{

}

#ifdef PROXY_STRICT_IP

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
#endif

void QnServerMessageProcessor::updateResource(QnResourcePtr resource)
{
    QnMediaServerResourcePtr ownMediaServer = qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnMediaServerResource>();

    bool isServer = resource.dynamicCast<QnMediaServerResource>();
    bool isCamera = resource.dynamicCast<QnVirtualCameraResource>();
    bool isUser = resource.dynamicCast<QnUserResource>();

    if (!isServer && !isCamera && !isUser)
        return;

    //storing all servers' cameras too
    // If camera from other server - marking it
    
    if (isCamera) 
    {
        if (resource->getParentId() != ownMediaServer->getId())
            resource->addFlags( QnResource::foreigner );
        // update all known IP list
        QnVirtualCameraResourcePtr camRes = resource.dynamicCast<QnVirtualCameraResource>();
#ifdef PROXY_STRICT_IP
        updateAllIPList(camRes->getId(), camRes->getHostAddress());
#endif
    }

    if (isServer) 
    {
        if (resource->getId() != ownMediaServer->getId())
            resource->addFlags( QnResource::foreigner );
        // update all known IP list
#ifdef PROXY_STRICT_IP
        updateAllIPList(resource->getId(), resource.dynamicCast<QnMediaServerResource>()->getNetAddrList());
#endif
    }

    bool needUpdateServer = false;
    // We are always online
    if (isServer && resource->getGuid() == serverGuid()) {
        if (resource->getStatus() != QnResource::Online) {
            qWarning() << "XYZ1: Received message that our status is " << resource->getStatus();
            resource->setStatus(QnResource::Online);
        }
    }

    if (QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId()))
        ownResource->update(resource);
    else
        qnResPool->addResource(resource);

    if (isServer && resource->getGuid() == serverGuid())
        syncStoragesToSettings(ownMediaServer);

}

void QnServerMessageProcessor::afterRemovingResource(const QnId& id)
{
    QnCommonMessageProcessor::afterRemovingResource(id);
#ifdef PROXY_STRICT_IP
    updateAllIPList(id, QList<QString>());
    QMutexLocker lock(&m_mutexAddrList);
    m_addrById.remove(id);
#endif
}

void QnServerMessageProcessor::init(ec2::AbstractECConnectionPtr connection)
{
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnServerMessageProcessor::at_remotePeerFound );
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnServerMessageProcessor::at_remotePeerLost );
    QnCommonMessageProcessor::init(connection);
}

#ifdef PROXY_STRICT_IP
bool QnServerMessageProcessor::isKnownAddr(const QString& addr) const
{
    QMutexLocker lock(&m_mutexAddrList);
    //return true;
    return m_allIPAddress.contains(addr);
}
#endif

/*
* EC2 related processing. Need move to other class
*/

void QnServerMessageProcessor::at_remotePeerFound(QnId id, bool isClient, bool isProxy)
{
    QnResourcePtr res = qnResPool->getResourceById(id);
    if (res)
        res->setStatus(QnResource::Online);

}

void QnServerMessageProcessor::at_remotePeerLost(QnId id, bool isClient, bool isProxy)
{
    QnResourcePtr res = qnResPool->getResourceById(id);
    if (res) {
        res->setStatus(QnResource::Offline);
        if (isClient) {
            // This media server hasn't own DB
            foreach(QnResourcePtr camera, qnResPool->getAllCameras(res))
                camera->setStatus(QnResource::Offline);
        }
    }
}

void QnServerMessageProcessor::onResourceStatusChanged(QnResourcePtr resource, QnResource::Status status)
{
    resource->setStatus(status, true);
}
