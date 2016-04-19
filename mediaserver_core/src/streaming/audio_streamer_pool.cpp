#include "audio_streamer_pool.h"
#include "core/resource/camera_resource.h"
#include "utils/common/unused.h"
#include "core/resource_management/resource_pool.h"
#include "camera/camera_pool.h"

QnAudioStreamerPool::QnAudioStreamerPool()
{
}

bool QnAudioStreamerPool::startStreamToResource(const QString& clientId, const QString& resourceId)
{
    auto res = qnResPool->getResourceById(QnUuid::fromStringSafe(resourceId));
    if(!res)
    {
        qDebug() << "2WAY AUDIO: Resource Not fouhd";
        return false;
    }

    auto resource = res.dynamicCast<QnSecurityCamResource>();

    if(!resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
    {
        qDebug() << "2WAY AUDIO: No capabilty to transmit audio";
        return false;
    }

    auto sourceCamResources = qnResPool->getResourcesWithTypeId(
        qnResTypePool->desktopCameraResourceType()->getId());

    qDebug() << "2WAY AUDIO: Desktop cameras count" << sourceCamResources.size();
    qDebug() << "2WAY AUDIO: Client id" << clientId;

    QnVideoCameraPtr sourceCam;
    for(const auto& res: sourceCamResources)
    {
        qDebug() << "2WAY AUDIO: Desktop camera unique id:" << res->getUniqueId();
        if(res->getUniqueId() == clientId)
        {
            qDebug() << "2WAY AUDIO: Desktop camera found";
            sourceCam = qnCameraPool->getVideoCamera(res);
            break;
        }
    }

    if(!sourceCam)
    {
        qDebug() << "2WAY AUDIO: No desktop camera has been found";
        return false;
    }

    sourceCam->inUse(this);
    auto reader = sourceCam->getLiveReader(QnServer::HiQualityCatalog);
    auto transmitter = resource->getAudioTransmitter();
    if(!transmitter)
        return false;

    qDebug() << "Setting up data transmitter";
    reader->addDataProcessor(transmitter);
    reader->startIfNotRunning();

    return true;
}

bool QnAudioStreamerPool::stopStreamToResource(const QString& clientId, const QString& resourceId)
{
    QN_UNUSED(clientId);

    //auto resource = qnResPool->getResourceById(resourceId);

    return false;
}
