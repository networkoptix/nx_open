#pragma once

#include <nx/utils/singleton.h>
#include <business/business_fwd.h>
#include <core/dataprovider/spush_media_stream_provider.h>
#include <utils/common/request_param.h>
#include <common/common_module_aware.h>
#include <camera/video_camera.h>

class QnAbstractAudioTransmitter;

class QnAudioStreamerPool:
    public Singleton<QnAudioStreamerPool>,
    public QnCommonModuleAware
{

public:
    QnAudioStreamerPool(QnCommonModule* commonModule);

    enum class Action
    {
        Start,
        Stop
    };

    bool startStopStreamToResource(const QnUuid& clientId, const QnUuid& resourceId, Action action, QString& error, const QnRequestParams &params);
    bool startStopStreamToResource(QnAbstractStreamDataProviderPtr desktopDataProvider, const QnUuid& resourceId, Action action, QString &error);

    QnAbstractStreamDataProviderPtr getActionDataProvider(const QnAbstractBusinessActionPtr &action);
    bool destroyActionDataProvider(const QnAbstractBusinessActionPtr &action);

private:
    QString calcActionUniqueKey(const QnAbstractBusinessActionPtr &action) const;

    QnVideoCameraPtr getTransmitSource(const QnUuid& clientId) const;
    QnSecurityCamResourcePtr getTransmitDestination(const QnUuid& resourceId) const;
private:
    QnMutex m_prolongedProvidersMutex;
    QMap<QString, QnAbstractStreamDataProviderPtr> m_actionDataProviders;
    QMap<QString, std::shared_ptr<QnAbstractAudioTransmitter>> m_proxyTransmitters;

};

