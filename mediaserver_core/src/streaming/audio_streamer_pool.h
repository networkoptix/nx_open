#pragma once

#include <nx/utils/singleton.h>
#include <nx/vms/event/event_fwd.h>
#include <providers/spush_media_stream_provider.h>
#include <utils/common/request_param.h>
#include <common/common_module_aware.h>
#include <camera/video_camera.h>
#include <nx/mediaserver/server_module_aware.h>

class QnAbstractAudioTransmitter;
namespace nx { namespace mediaserver { namespace resource { class Camera; } }}

class QnAudioStreamerPool:
    public Singleton<QnAudioStreamerPool>,
    public nx::mediaserver::ServerModuleAware
{

public:
    QnAudioStreamerPool(QnMediaServerModule* serverModule);

    enum class Action
    {
        Start,
        Stop
    };

    bool startStopStreamToResource(const QString& sourceId,
        const QnUuid& resourceId,
        Action action,
        QString& error,
        const QnRequestParams& params);
    bool startStopStreamToResource(QnAbstractStreamDataProviderPtr desktopDataProvider,
        const QnUuid& resourceId,
        Action action,
        QString& error);

    QnAbstractStreamDataProviderPtr getActionDataProvider(const nx::vms::event::AbstractActionPtr& action);
    bool destroyActionDataProvider(const nx::vms::event::AbstractActionPtr& action);

private:
    QString calcActionUniqueKey(const nx::vms::event::AbstractActionPtr& action) const;

    QnVideoCameraPtr getTransmitSource(const QString& sourceId) const;
    nx::mediaserver::resource::CameraPtr getTransmitDestination(const QnUuid& resourceId) const;
private:
    QnMutex m_prolongedProvidersMutex;
    QMap<QString, QnAbstractStreamDataProviderPtr> m_actionDataProviders;
    QMap<QString, std::shared_ptr<QnAbstractAudioTransmitter>> m_proxyTransmitters;

};

