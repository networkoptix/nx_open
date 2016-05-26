#include "audio_streamer_pool.h"
#include "core/resource/camera_resource.h"
#include "utils/common/unused.h"
#include "core/resource_management/resource_pool.h"
#include <camera/camera_pool.h>

namespace
{
    QnMutex* getLock(const QnUuid& key)
    {
        static QnMutex internalMutex;
        static QMap<QnUuid, std::shared_ptr<QnMutex>> locks;
        
        QnMutexLocker lock(&internalMutex);
        std::shared_ptr<QnMutex>& value = locks[key];
        if (value == nullptr)
            value.reset(new QnMutex());
        return value.get();
    }

    QnVideoCameraPtr getTransmitSource(const QnUuid& clientId)
    {
        for (const auto& res: qnResPool->getResourcesWithFlag(Qn::desktop_camera))
        {
            if (QnUuid::fromStringSafe(res->getUniqueId()) == clientId ||
                res->getId() == clientId)
            {
                return qnCameraPool->getVideoCamera(res);
            }
        }
        return QnVideoCameraPtr();
    }

    QnSecurityCamResourcePtr getTransmitDestination(const QnUuid& resourceId)
    {
        auto resource = qnResPool->getResourceById<QnSecurityCamResource>(resourceId);
        if (!resource)
            return QnSecurityCamResourcePtr();
        if (!resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
            return QnSecurityCamResourcePtr();
        return resource;
    }
}

QnAudioStreamerPool::QnAudioStreamerPool()
{

}

bool QnAudioStreamerPool::startStopStreamToResource(const QnUuid& clientId, const QnUuid& resourceId, Action action, QString &error)
{
    auto resource = getTransmitDestination(resourceId);
    if (!resource)
    {
        error = lit("Can't find camera with id '%1'")
            .arg(resourceId.toString());
        return false;
    }

    auto sourceCam = getTransmitSource(clientId);
    if (!sourceCam)
    {
        error = lit("Client PC with id '%1' is not found.")
            .arg(clientId.toString());
        return false;
    }

    QnLiveStreamProviderPtr desktopDataProvider = sourceCam->getLiveReader(QnServer::HiQualityCatalog);
    if(!desktopDataProvider)
    {
        error = lit("Client PC with id '%1' is found but not ready to stream audio yet.")
            .arg(clientId.toString());
        return false;
    }

    QnAudioTransmitterPtr transmitter = resource->getAudioTransmitter();
    if (!transmitter)
    {
        error = lit("Camera '%1' does not support 2-way audio.")
            .arg(resourceId.toString());
        return false;
    }

    // This lock avoid to start and stop same transmitter in the same time
    QnMutexLocker lock(getLock(resourceId));
    if (action == Action::Start)
        transmitter->subscribe(desktopDataProvider);
    else
        transmitter->unsubscribe(desktopDataProvider);

    return true;
}
