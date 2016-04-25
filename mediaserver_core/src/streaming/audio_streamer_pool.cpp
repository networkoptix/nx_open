#include "audio_streamer_pool.h"
#include "core/resource/camera_resource.h"
#include "utils/common/unused.h"
#include "core/resource_management/resource_pool.h"

namespace
{
    const QString kChooseClientAutomatically("auto");

    QnMutex* getLock(const QString& key)
    {
        static QnMutex internalMutex;
        static QMap<QString, std::shared_ptr<QnMutex>> locks;
        
        QnMutexLocker lock(&internalMutex);
        std::shared_ptr<QnMutex>& value = locks[key];
        if (value == nullptr)
            value.reset(new QnMutex());
        return value.get();
    }
}

QnAudioStreamerPool::QnAudioStreamerPool()
{

}

QnVideoCameraPtr QnAudioStreamerPool::getTransmitSource(const QString& clientId) const
{
    auto sourceCamResources = qnResPool->getResourcesWithFlag(Qn::desktop_camera);

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
    auto resource = qnResPool->getResourceById<QnSecurityCamResource>(QnUuid::fromStringSafe(resourceId));

    if (!resource)
        return QnSecurityCamResourcePtr(0);

    if (!resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
        return QnSecurityCamResourcePtr(0);

    return resource;
}

bool QnAudioStreamerPool::startStopStreamToResource(const QString& clientId, const QString& resourceId, Action action, QString &error)
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

    // This lock avoid to start and stop same transmitter in the same time
    QnMutexLocker lock(getLock(resourceId));
    if (action == Action::Start)
    {
        transmitter->stop(); //< in case if still stopping from a previous call
        sourceCam->inUse(transmitter.get());
        reader->addDataProcessor(transmitter.get());
        transmitter->start();
        reader->startIfNotRunning();
    }
    else
    {
        sourceCam->notInUse(transmitter.get());
        transmitter->pleaseStop();
        reader->removeDataProcessor(transmitter.get());
    }

    return true;
}
