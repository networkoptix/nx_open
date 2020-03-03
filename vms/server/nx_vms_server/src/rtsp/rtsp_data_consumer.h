#pragma once

#include <chrono>
#include <optional>

#include <QtCore/QElapsedTimer>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/abstract_data_packet.h>
#include <rtsp/rtsp_ffmpeg_encoder.h>
#include <utils/common/adaptive_sleep.h>
#include <camera/video_camera.h>

#include <nx/analytics/metadata_logger.h>
#include <nx/metrics/streams_metric_helper.h>
#include <nx/vms/server/put_in_order_data_provider.h>

class QnRtspConnectionProcessor;

static const int MAX_RTP_CHANNELS = 32;
static const int CLOCK_FREQUENCY = 1000000;

static const quint8 RTP_METADATA_CODE = 126;
static const QString RTP_METADATA_GENERIC_STR("ffmpeg-metadata");

static const int RTSP_MIN_SEEK_INTERVAL = 1000 * 30; // 30 ms as min seek interval

class QnRtspDataConsumer:
    public QnAbstractDataConsumer
{
public:
    static const int MAX_STREAMING_SPEED = INT_MAX;

    QnRtspDataConsumer(QnRtspConnectionProcessor* owner);
    virtual ~QnRtspDataConsumer() override;

    void pauseNetwork();
    void resumeNetwork();

    void setLastSendTime(qint64 time);
    qint64 lastSendTime() const;
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
    void sendReorderedData();
private:
    void recvRtcpReport(nx::network::AbstractDatagramSocket* rtcpSocket);
    bool needData(const QnAbstractDataPacketPtr& data) const;
    void switchQualityIfNeeded(bool isSecondaryProvider);
    void processMediaData(const QnAbstractMediaDataPtr& nonConstData);
    void flushReorderingBuffer();
    void sendBufferViaTcp(std::optional<int64_t> timestampForLogging = std::nullopt);

private:
    bool m_gotLivePacket;
    QByteArray m_codecCtxData;
    QElapsedTimer m_timer;
    nx::utils::ElapsedTimer m_keepAliveTimer;
    QnRtspConnectionProcessor* const m_owner;
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
    int m_packetSent;
    int m_liveMarker;
    MediaQuality m_liveQuality;
    MediaQuality m_newLiveQuality;

    std::optional<std::chrono::time_point<std::chrono::steady_clock>> m_lastSendBufferViaTcpTime;

    int m_streamingSpeed;
    bool m_multiChannelVideo;
    QnAdaptiveSleep m_adaptiveSleep;
    bool m_useUTCTime; // use absolute UTC file for RTP (used for proprietary format)
    int m_fastChannelZappingSize;

    QnByteArray m_sendBuffer;
    bool m_someDataIsDropped;

    int m_framesSinceRangeCheck;
    QByteArray m_prevRangeHeader;
    quint32 m_videoChannels;
    std::array<bool, CL_MAX_CHANNELS> m_needKeyData;
    nx::vms::api::StreamDataFilters m_streamDataFilter{nx::vms::api::StreamDataFilter::media};

    /**
     * Last live frames timestamps.
     * m_lastLiveFrameTime[0] - last primary stream frame timestamp and
     * m_lastLiveFrameTime[1] - last secondary stream frame timestamp
     */
    int64_t m_lastLiveFrameTime[2] {0};

    std::unique_ptr<nx::analytics::MetadataLogger> m_primaryProcessDataLogger;
    std::unique_ptr<nx::analytics::MetadataLogger> m_secondaryProcessDataLogger;
    std::unique_ptr<nx::analytics::MetadataLogger> m_primaryPutDataLogger;
    std::unique_ptr<nx::analytics::MetadataLogger> m_secondaryPutDataLogger;
    nx::vms::metrics::StreamMetricHelper m_streamMetricHelper;
    std::unique_ptr<nx::vms::server::SimpleReorderer> m_reorderingProvider;
};
