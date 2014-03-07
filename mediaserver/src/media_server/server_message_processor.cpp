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
    
    if (isCamera && resource->getParentId() != ownMediaServer->getId())
        resource->addFlags( QnResource::foreigner );

    if (isServer && resource->getId() != ownMediaServer->getId())
        resource->addFlags( QnResource::foreigner );

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

void QnServerMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    // TODO: implement me

    // TODO: #EC2

    /*case Qn::Message_Type_KvPairChange: {
        updateKvPairs(message.kvPairs);
        break;
    }*/

}

void QnServerMessageProcessor::init(ec2::AbstractECConnectionPtr connection)
{
    QnCommonMessageProcessor::init(connection);
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerFound, this, &QnServerMessageProcessor::at_remotePeerFound );
    connect( connection.get(), &ec2::AbstractECConnection::remotePeerLost, this, &QnServerMessageProcessor::at_remotePeerLost );
}

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
            foreach(QnResourcePtr camera, qnResPool->getAllEnabledCameras(res, QnResourcePool::AllResources))
                camera->setStatus(QnResource::Offline);
        }
    }
}

void QnServerMessageProcessor::onResourceStatusChanged(QnResourcePtr resource, QnResource::Status status)
{
    resource->setStatus(status, true);
}
