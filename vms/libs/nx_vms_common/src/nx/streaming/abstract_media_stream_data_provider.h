// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include <nx/media/config.h>
#include <nx/media/media_data_packet.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/media_stream_statistics.h>
#include <nx/utils/value_cache.h>
#include <utils/camera/camera_diagnostics.h>

class NX_VMS_COMMON_API QnAbstractMediaStreamDataProvider:
    public QnAbstractStreamDataProvider
{
public:
    static void setMediaStatisticsWindowSize(std::chrono::microseconds value);
    static void setMediaStatisticsMaxDurationInFrames(int value);

    explicit QnAbstractMediaStreamDataProvider(const QnResourcePtr& resource);
    virtual ~QnAbstractMediaStreamDataProvider();

    const QnMediaStreamStatistics* getStatistics(int channel) const;
    qint64 bitrateBitsPerSecond() const;
    float getFrameRate() const;
    float getAverageGopSize() const;

    virtual CodecParametersConstPtr getCodecContext() const;

    /**
     * Tests connection to a media stream. Blocks for media stream open attempt. Default
     * implementation returns notImplemented.
     */
    virtual CameraDiagnostics::Result diagnoseMediaStreamConnection();

    virtual bool hasThread() const { return true; }
    virtual bool reinitResourceOnStreamError() const { return true; }
    bool isConnectionLost() const;

protected:
    virtual void sleepIfNeeded() {}

    virtual void beforeRun();
    virtual void afterRun();

    virtual bool beforeGetData() { return true; }

    void checkAndFixTimeFromCamera(const QnAbstractMediaDataPtr& data);
    void resetTimeCheck();
    void onEvent(CameraDiagnostics::Result event);
    void resetMediaStatistics();
protected:
    QnMediaStreamStatistics m_stat[CL_MAX_CHANNEL_NUMBER];
    std::array<int, CL_MAX_CHANNEL_NUMBER> m_gotKeyFrame;
    QnMediaResourcePtr m_mediaResource;

    virtual void setNeedKeyData();
    virtual void setNeedKeyData(int channel);
    virtual bool needKeyData(int channel) const;
    virtual bool needKeyData() const;

private:
    int getNumberOfChannels() const;

private:
    nx::utils::CachedValue<int> m_numberOfChannels;
    qint64 m_lastMediaTime[CL_MAX_CHANNELS + 1]; //< max video channels + audio channel
    bool m_isCamera;
    std::atomic<int> m_numberOfErrors{};
};

typedef std::shared_ptr<QnAbstractMediaStreamDataProvider> QnAbstractMediaStreamDataProviderPtr;
