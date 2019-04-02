#pragma once

#include <QtNetwork/QHostAddress>
#include <QtCore/QElapsedTimer>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/abstract_data_packet.h>
#include <utils/media/externaltimesource.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>
#include <utils/common/adaptive_sleep.h>
#include <camera/video_camera.h>

#include <nx/analytics/metadata_logger.h>

class QnRtspConnectionProcessor;

static const int MAX_RTP_CHANNELS = 32;
static const int CLOCK_FREQUENCY = 1000000;

static const quint8 RTP_METADATA_CODE = 126;
static const QString RTP_METADATA_GENERIC_STR("ffmpeg-metadata");

static const int RTSP_MIN_SEEK_INTERVAL = 1000 * 30; // 30 ms as min seek interval

class QnRtspDataConsumer:
    public QnAbstractDataConsumer, public QnlTimeSource
{
public:
    static const int MAX_STREAMING_SPEED = INT_MAX;

    QnRtspDataConsumer(QnRtspConnectionProcessor* owner);
    virtual ~QnRtspDataConsumer() override;

    void pauseNetwork();
    void resumeNetwork();

    void setLastSendTime(qint64 time);
    void setWaitCSeq(qint64 newTime, int sceq);
    virtual void putData(const QnAbstractDataPacketPtr& data);
    virtual bool canAcceptData() const;
    void setLiveMode(bool value);
    int copyLastGopFromCamera(
        QnVideoCameraPtr camera,
        nx::vms::api::StreamIndex streamIndex,
        qint64 skipTime,
        bool iFramesOnly);
    QnMutex* dataQueueMutex();
    void setSingleShotMode(bool value);

    virtual qint64 getDisplayedTime() const;
    virtual qint64 getCurrentTime() const;
    virtual qint64 getNextTime() const;
    virtual qint64 getExternalTime() const;

    qint64 lastQueuedTime();
    void setSecondaryDataProvider(QnAbstractStreamDataProvider* value);

    // Marker increased after play/seek operation
    void setLiveMarker(int marker);
    void setLiveQuality(MediaQuality liveQuality);
    virtual void clearUnprocessedData() override;

    // put data without mutex. Used for RTSP connection after lockDataQueue
    void addData(const QnAbstractMediaDataPtr& data);
    void setStreamingSpeed(int speed);
    void setMultiChannelVideo(bool value);
    void setUseUTCTime(bool value);
    void setAllowAdaptiveStreaming(bool value);
    void setResource(const QnResourcePtr& resource);
    std::chrono::milliseconds timeFromLastReceiverReport();

    nx::vms::api::StreamDataFilters streamDataFilter() const;
    void setStreamDataFilter(nx::vms::api::StreamDataFilters filter);

    virtual bool needConfigureProvider() const override { return true;  }
protected:
    //QnMediaContextPtr getGeneratedContext(AVCodecID compressionType);
    virtual bool processData(const QnAbstractDataPacketPtr& data);

    void createDataPacketTCP(QnByteArray& sendBuffer, const QnAbstractMediaDataPtr& media, int rtpTcpChannel);

    // delay streaming. Used for realtime mode streaming
    void doRealtimeDelay(QnConstAbstractMediaDataPtr media);

    bool isMediaTimingsSlow() const;
    void setLiveQualityInternal(MediaQuality quality);
    qint64 dataQueueDuration();
    void sendMetadata(const QByteArray& metadata);
    void getEdgePackets(
        const QnDataPacketQueue::RandomAccess<>& unsafeQueue,
        qint64& firstVTime,
        qint64& lastVTime,
        bool checkLQ) const;
    void sendRangeHeaderIfChanged();
    void cleanupQueueToPos(QnDataPacketQueue::RandomAccess<>& unsafeQueue, int lastIndex, quint32 ch);
    void setNeedKeyData();

private:
    void recvRtcpReport(nx::network::AbstractDatagramSocket* rtcpSocket);
    bool needData(const QnAbstractDataPacketPtr& data) const;
    MediaQuality preferredLiveStreamQuality(int channelNumber) const;

private:
    //QMap<AVCodecID, QnMediaContextPtr> m_generatedContext;
    bool m_gotLivePacket;
    QByteArray m_codecCtxData;
    //QMap<int, QList<int> > m_ctxSended;
    QElapsedTimer m_timer;
    nx::utils::ElapsedTimer m_keepAliveTimer;
    //quint16 m_sequence[MAX_RTP_CHANNELS];
    //qint64 m_firstRtpTime[MAX_RTP_CHANNELS];
    QnRtspConnectionProcessor* m_owner;
    qint64 m_lastSendTime;
    qint64 m_rtStartTime; // used for realtime streaming mode
    qint64 m_lastRtTime; // used for realtime streaming mode
    qint64 m_lastMediaTime; // same as m_lastSendTime, but show real timestamp for LIVE video (m_lastSendTime always returns DATETIME_NOW for live)
    QnMutex m_mutex;
    QnMutex m_qualityChangeMutex;
    int m_waitSCeq;
    bool m_liveMode;
    bool m_pauseNetwork;
    QnMutex m_dataQueueMtx;
    bool m_singleShotMode;
    int m_packetSended;
    int m_liveMarker;
    MediaQuality m_liveQuality;
    MediaQuality m_newLiveQuality;

    static QHash<QHostAddress, qint64> m_lastSwitchTime;
    static QSet<QnRtspDataConsumer*> m_allConsumers;
    static QnMutex m_allConsumersMutex;
    int m_streamingSpeed;
    bool m_multiChannelVideo;
    QnAdaptiveSleep m_adaptiveSleep;
    bool m_useUTCTime; // use absolute UTC file for RTP (used for proprietary format)
    int m_fastChannelZappingSize;

    qint64 m_firstLiveTime;
    qint64 m_lastLiveTime;
    QElapsedTimer m_liveTimer;
    mutable QnMutex m_liveTimingControlMtx;
    bool m_allowAdaptiveStreaming;

    QnByteArray m_sendBuffer;
    bool m_someDataIsDropped;
    qint64 m_previousRtpTimestamp;
    qint64 m_previousScaledRtpTimestamp;

    int m_framesSinceRangeCheck;
    QByteArray m_prevRangeHeader;
    quint32 m_videoChannels;
    std::array<bool, CL_MAX_CHANNELS> m_needKeyData;
    nx::vms::api::StreamDataFilters m_streamDataFilter{nx::vms::api::StreamDataFilter::media};

    MediaQuality m_currentQuality[CL_MAX_CHANNELS]{MEDIA_Quality_None};
    /**
     * Last live frames timestamps per each possible channel.
     * m_lastLiveFrameTime[CHANNEL_NUMBER][0] - last primary stream frame timestamp and
     * m_lastLiveFrameTime[CHANNEL_NUMBER][1] - last secondary stream frame timestamp for the
     * given CHANNEL_NUMBER.
     */
    int64_t m_lastLiveFrameTime[CL_MAX_CHANNELS][2]{{0}};
    bool m_isLive = false;

    std::unique_ptr<nx::analytics::MetadataLogger> m_primaryLogger;
    std::unique_ptr<nx::analytics::MetadataLogger> m_secondaryLogger;
};
