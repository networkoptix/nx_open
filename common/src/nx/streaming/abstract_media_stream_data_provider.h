#pragma once

#include <QtCore/QSharedPointer>
#include <nx/streaming/media_stream_statistics.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <utils/camera/camera_diagnostics.h>
#include <nx/streaming/media_data_packet.h>

class QnResourceVideoLayout;
class QnResourceAudioLayout;

#define MAX_LIVE_FPS 10000000.0
#define MAX_LOST_FRAME 2

class QnAbstractMediaStreamDataProvider: public QnAbstractStreamDataProvider
{
    Q_OBJECT;

public:
    explicit QnAbstractMediaStreamDataProvider(const QnResourcePtr& res);
    virtual ~QnAbstractMediaStreamDataProvider();

    const QnMediaStreamStatistics* getStatistics(int channel) const;
    int getNumberOfChannels() const;
    float getBitrateMbps() const;
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

protected:
    virtual void sleepIfNeeded() {}

    virtual void beforeRun();
    virtual void afterRun();

    virtual bool beforeGetData() { return true; }

    // If the function returns false, we do not put the result into the queues.
    virtual bool afterGetData(const QnAbstractDataPacketPtr& data);

    void checkTime(const QnAbstractMediaDataPtr& data);
    void checkAndFixTimeFromCamera(const QnAbstractMediaDataPtr& data);
    void resetTimeCheck();

protected:
    QnMediaStreamStatistics m_stat[CL_MAX_CHANNEL_NUMBER];
    int m_gotKeyFrame[CL_MAX_CHANNEL_NUMBER];

    int mFramesLost;
    QnResourcePtr m_mediaResource;

private:
    mutable int m_numberOfchannels;
    qint64 m_lastMediaTime[CL_MAX_CHANNELS + 1]; //< max video channels + audio channel
    bool m_isCamera;
    qint64 m_unloopingPeriodStartUs = 0;
};

typedef QSharedPointer<QnAbstractMediaStreamDataProvider> QnAbstractMediaStreamDataProviderPtr;
