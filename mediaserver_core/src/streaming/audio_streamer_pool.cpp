#include "audio_streamer_pool.h"
#include <core/resource/camera_resource.h>
#include <nx/utils/unused.h>
#include <core/resource_management/resource_pool.h>
#include <camera/camera_pool.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <core/resource/avi/avi_resource.h>
#include <proxy/2wayaudio/proxy_audio_transmitter.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>

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
}

QnAudioStreamerPool::QnAudioStreamerPool(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

QnVideoCameraPtr QnAudioStreamerPool::getTransmitSource(const QString& sourceId) const
{
    if (auto camera = resourcePool()->getResourceByUniqueId(sourceId))
    {
        NX_ASSERT(camera->hasFlags(Qn::desktop_camera));
        if (camera->hasFlags(Qn::desktop_camera))
            return qnCameraPool->getVideoCamera(camera);
    }
    return QnVideoCameraPtr();
}

QnSecurityCamResourcePtr QnAudioStreamerPool::getTransmitDestination(const QnUuid& resourceId) const
{
    auto resource = resourcePool()->getResourceById<QnSecurityCamResource>(resourceId);
    if (!resource)
        return QnSecurityCamResourcePtr();
    if (!resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
        return QnSecurityCamResourcePtr();
    return resource;
}

bool QnAudioStreamerPool::startStopStreamToResource(const QString& sourceId,
    const QnUuid& resourceId,
    Action action,
    QString& error,
    const QnRequestParams& params)
{
    auto resource = getTransmitDestination(resourceId);
    if (!resource)
    {
        error = lit("Can't find camera with id '%1'")
            .arg(resourceId.toString());
        return false;
    }

    auto sourceCam = getTransmitSource(sourceId);
    if (!sourceCam)
    {
        error = lit("Client PC with id '%1' is not found.")
            .arg(sourceId);
        return false;
    }

    QnLiveStreamProviderPtr desktopDataProvider = sourceCam->getLiveReader(QnServer::HiQualityCatalog);
    if(!desktopDataProvider)
    {
        error = lit("Client PC with id '%1' is found but not ready to stream audio yet.")
            .arg(sourceId);
        return false;
    }

    QnMediaServerResourcePtr mServer = resource->getParentServer();
    if(!mServer)
    {
        error = lit("Internal server error: can't find camera's server");
        return false;
    }

    QnAudioTransmitterPtr transmitter;
    if (mServer->getId() == commonModule()->moduleGUID())
    {
        transmitter = resource->getAudioTransmitter();
    }
    else
    {
        QString key = sourceId + resourceId.toString();
        auto& proxyTransmitter = m_proxyTransmitters[key];
        if (!proxyTransmitter)
            proxyTransmitter.reset(new QnProxyAudioTransmitter(resource, params));
        transmitter = proxyTransmitter;
    }

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
        transmitter->unsubscribe(desktopDataProvider.data());

    return true;
}

bool QnAudioStreamerPool::startStopStreamToResource(QnAbstractStreamDataProviderPtr desktopDataProvider, const QnUuid& resourceId, Action action, QString &error)
{
    auto resource = getTransmitDestination(resourceId);
    if (!resource)
    {
        error = lit("Can't find camera with id '%1'")
            .arg(resourceId.toString());
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
        transmitter->unsubscribe(desktopDataProvider.data());

    return true;
}

QString QnAudioStreamerPool::calcActionUniqueKey(const nx::vms::event::AbstractActionPtr& action) const
{
    return action->getRuleId().toString();
}

QnAbstractStreamDataProviderPtr QnAudioStreamerPool::getActionDataProvider(const nx::vms::event::AbstractActionPtr& action)
{
    QnMutexLocker lock(&m_prolongedProvidersMutex);
    auto type = action->actionType();
    auto params = action->getParams();
    auto actionKey = calcActionUniqueKey(action);

    if (m_actionDataProviders.contains(actionKey))
        return m_actionDataProviders[actionKey];

    QnAbstractStreamDataProviderPtr provider;
    if (type == nx::vms::event::playSoundAction)
    {
        const auto filePath = lit("dbfile://notifications/") + params.url;
        QnAviResourcePtr resource(new QnAviResource(filePath));
        resource->setCommonModule(commonModule());
        resource->setStatus(Qn::Online);
        provider.reset(resource->createDataProvider(Qn::ConnectionRole::CR_Default));
    }
    else
    {
        return QnAbstractStreamDataProviderPtr();
    }

    m_actionDataProviders[actionKey] = provider;

    return provider;
}

bool QnAudioStreamerPool::destroyActionDataProvider(const nx::vms::event::AbstractActionPtr& action)
{
    QnMutexLocker lock(&m_prolongedProvidersMutex);
    auto actionKey = calcActionUniqueKey(action);
    if(m_actionDataProviders.contains(actionKey))
        m_actionDataProviders.remove(actionKey);

    return true;
}
