#pragma once

#include <nx/utils/singleton.h>
#include <business/business_fwd.h>
#include <core/dataprovider/spush_media_stream_provider.h>
#include <utils/common/request_param.h>

class QnAbstractAudioTransmitter;

class QnAudioStreamerPool : public Singleton<QnAudioStreamerPool>
{

public:
    QnAudioStreamerPool();

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

private:
    QnMutex m_prolongedProvidersMutex;
    QMap<QString, QnAbstractStreamDataProviderPtr> m_actionDataProviders;
    QMap<QString, std::shared_ptr<QnAbstractAudioTransmitter>> m_proxyTransmitters;

};

