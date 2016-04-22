#include "audio_streamer_pool.h"
#include "core/resource/camera_resource.h"
#include "utils/common/unused.h"
#include "core/resource_management/resource_pool.h"

namespace
{
    const QString kChooseClientAutomatically("auto");
}

QnAudioStreamerPool::QnAudioStreamerPool()
{

}

QnVideoCameraPtr QnAudioStreamerPool::getTransmitSource(const QString& clientId) const
{
    auto sourceCamResources = qnResPool->getResourcesWithTypeId(
        qnResTypePool->desktopCameraResourceType()->getId());

    QnVideoCameraPtr sourceCam;
    for (const auto& res: sourceCamResources)
    {
        if ((res->getUniqueId() == clientId) || clientId == kChooseClientAutomatically)
        {
            sourceCam = qnCameraPool->getVideoCamera(res);
            break;
        }
    }

    return sourceCam;
}

QnSecurityCamResourcePtr QnAudioStreamerPool::getTransmitDestination(const QString& resourceId) const
{
    auto res = qnResPool->getResourceById(QnUuid::fromStringSafe(resourceId));
    if (!res)
    {
        return QnSecurityCamResourcePtr(0);
    }

    auto resource = res.dynamicCast<QnSecurityCamResource>();

    if (!resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
    {
        return QnSecurityCamResourcePtr(0);
    }

    return resource;
}

bool QnAudioStreamerPool::startStreamToResource(const QString& clientId, const QString& resourceId, QString& error)
{
    auto resource = getTransmitDestination(resourceId);
    if (!resource)
    {
        error = lit("Can't find resource with id '%1'")
            .arg(resourceId);
        return false;
    }

    auto sourceCam = getTransmitSource(clientId);
    if (!sourceCam)
    {
        error = lit("Can't find client with id '%1'")
            .arg(clientId);
        return false;
    }

    auto reader = sourceCam->getLiveReader(QnServer::HiQualityCatalog);
    if(!reader)
    {
        error = lit("Unable to obtaine live reader for client '%1'")
            .arg(clientId);
        return false;
    }

    auto transmitter = resource->getAudioTransmitter();
    if (!transmitter)
    {
        error = lit("Unable to obtain audio transmitter for resource '%1'")
            .arg(resourceId);
        return false;
    }

    sourceCam->inUse(transmitter.get());
    reader->addDataProcessor(transmitter.get());
    if(transmitter->isRunning())
        transmitter->stop();
    transmitter->start();
    reader->startIfNotRunning();

    return true;
}

bool QnAudioStreamerPool::stopStreamToResource(const QString& clientId, const QString& resourceId, QString &error)
{
    auto resource = getTransmitDestination(resourceId);
    if (!resource)
    {
        error = lit("Can't find resource with id '%1'")
            .arg(resourceId);
        return false;
    }

    auto sourceCam = getTransmitSource(clientId);
    if (!sourceCam)
    {
        error = lit("Can't find client with id '%1'")
            .arg(clientId);
        return false;
    }

    auto reader = sourceCam->getLiveReader(QnServer::HiQualityCatalog);
    auto transmitter = resource->getAudioTransmitter();
    if (!transmitter)
    {
        error = lit("Unable to obtain audio transmitter for resource '%1'")
            .arg(resourceId);
        return false;
    }

    sourceCam->notInUse(transmitter.get());
    transmitter->stop();
    reader->removeDataProcessor(transmitter.get());

    return true;
}
