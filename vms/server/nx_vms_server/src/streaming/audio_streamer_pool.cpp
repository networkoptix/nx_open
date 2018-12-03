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
#include <nx/vms/server/resource/camera.h>
#include <media_server/media_server_module.h>
#include <core/dataprovider/data_provider_factory.h>

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

QnAudioStreamerPool::QnAudioStreamerPool(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QnVideoCameraPtr QnAudioStreamerPool::getTransmitSource(const QString& sourceId) const
{
    if (auto camera = resourcePool()->getResourceByUniqueId(sourceId))
    {
        NX_ASSERT(camera->hasFlags(Qn::desktop_camera));
        if (camera->hasFlags(Qn::desktop_camera))
            return videoCameraPool()->getVideoCamera(camera);
    }
    return QnVideoCameraPtr();
}

nx::vms::server::resource::CameraPtr QnAudioStreamerPool::getTransmitDestination(const QnUuid& resourceId) const
{
    auto resource = resourcePool()->getResourceById<nx::vms::server::resource::Camera>(resourceId);
    if (!resource)
        return nx::vms::server::resource::CameraPtr();
    if (!resource->hasCameraCapabilities(Qn::AudioTransmitCapability))
        return nx::vms::server::resource::CameraPtr();
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
    if (mServer->getId() == moduleGUID())
    {
        transmitter = resource->getAudioTransmitter();
    }
    else
    {
        QString key = sourceId + resourceId.toString();
        auto& proxyTransmitter = m_proxyTransmitters[key];
        if (!proxyTransmitter)
        {
            proxyTransmitter.reset(new QnProxyAudioTransmitter(
                serverModule()->commonModule(), resource, params));
        }
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
    if (type == nx::vms::api::ActionType::playSoundAction)
    {
        const auto filePath = lit("dbfile://notifications/") + params.url;
        QnAviResourcePtr resource(new QnAviResource(filePath));
        resource->setCommonModule(serverModule()->commonModule());
        resource->setStatus(Qn::Online);
        provider.reset(serverModule()->dataProviderFactory()->createDataProvider(resource));
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
