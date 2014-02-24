#include "server_message_processor.h"
#include "core/resource_management/resource_pool.h"
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include "serverutil.h"


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

    // If the resource is mediaServer then ignore if not this server
    if (isServer && resource->getGuid() != serverGuid())
        return;

    //storing all servers' cameras too
    // If camera from other server - marking it
    if (isCamera && resource->getParentId() != ownMediaServer->getId())
        resource->addFlags( QnResource::foreigner );

    bool needUpdateServer = false;
    // We are always online
    if (isServer) {
        if (resource->getStatus() != QnResource::Online) {
            resource->setStatus(QnResource::Online);
        }
    }

    if (QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId(), QnResourcePool::AllResources))
        ownResource->update(resource);
    else
        qnResPool->addResource(resource);

    if (isServer)
        syncStoragesToSettings(ownMediaServer);
}

void QnServerMessageProcessor::onGotInitialNotification(const ec2::QnFullResourceData& fullData)
{
    // TODO: implement me

    case Qn::Message_Type_KvPairChange: {
        updateKvPairs(message.kvPairs);
        break;
    }

}
