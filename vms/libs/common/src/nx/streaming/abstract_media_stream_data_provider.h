#pragma once

#include <QtCore/QSharedPointer>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <utils/camera/camera_diagnostics.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/media_stream_statistics.h>
#include <nx/utils/value_cache.h>

class QnResourceVideoLayout;
class QnResourceAudioLayout;

class QnAbstractMediaStreamDataProvider: public QnAbstractStreamDataProvider
{
    Q_OBJECT

public:

    explicit QnAbstractMediaStreamDataProvider(const QnResourcePtr& res);
    virtual ~QnAbstractMediaStreamDataProvider();

    const QnMediaStreamStatistics* getStatistics(int channel) const;
    qint64 bitrateBitsPerSecond() const;
    float getFrameRate() const;
    float getAverageGopSize() const;

    virtual void setNeedKeyData();
    virtual void setNeedKeyData(int channel);
    virtual bool needKeyData(int channel) const;
    virtual bool needKeyData() const;

    virtual QnConstMediaContextPtr getCodecContext() const;

    /**
     * Tests connection to a media stream. Blocks for media stream open attempt. Default
     * implementation returns notImplemented.
     */
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection();

    virtual bool hasThread() const { return true; }
    virtual bool reinitResourceOnStreamError() const { return true; }
    bool isConnectionLost() const;
signals:
    void streamEvent(QnAbstractMediaStreamDataProvider* streamReader, CameraDiagnostics::Result result);
protected:
    virtual void sleepIfNeeded() {}

    virtual void beforeRun();
    virtual void afterRun();

    virtual bool beforeGetData() { return true; }

    void checkAndFixTimeFromCamera(const QnAbstractMediaDataPtr& data);
    void resetTimeCheck();
    void onEvent(std::chrono::microseconds timestamp, CameraDiagnostics::Result event);
protected:
    QnMediaStreamStatistics m_stat[CL_MAX_CHANNEL_NUMBER];
    int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];

    QnMediaResourcePtr m_mediaResource;
private:
    int getNumberOfChannels() const;
private:
    nx::utils::CachedValue<int> m_numberOfChannels;
    qint64 m_lastMediaTime[CL_MAX_CHANNELS + 1]; //< max video channels + audio channel
    bool m_isCamera;
    std::atomic<int> m_numberOfErrors{};
};

typedef QSharedPointer<QnAbstractMediaStreamDataProvider> QnAbstractMediaStreamDataProviderPtr;
