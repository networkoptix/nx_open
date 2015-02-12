#ifndef __RTSP_DATA_CONSUMER_H__
#define __RTSP_DATA_CONSUMER_H__

#include <QtNetwork/QHostAddress>
#include <QtCore/QElapsedTimer>

#include "core/dataconsumer/abstract_data_consumer.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/datapacket/abstract_data_packet.h"
#include "utils/network/rtpsession.h"
#include "utils/media/externaltimesource.h"
#include "rtsp/rtsp_ffmpeg_encoder.h"
#include "utils/common/adaptive_sleep.h"

class QnRtspConnectionProcessor;

static const int MAX_RTP_CHANNELS = 32;
static const int CLOCK_FREQUENCY = 1000000;

static const quint8 RTP_METADATA_CODE = 126;
static const QString RTP_METADATA_GENERIC_STR("ffmpeg-metadata");

static const int RTSP_MIN_SEEK_INTERVAL = 1000 * 30; // 30 ms as min seek interval

class QnRtspDataConsumer: public QnAbstractDataConsumer, public QnlTimeSource
{
public:
    static const int MAX_STREAMING_SPEED = INT_MAX;

    QnRtspDataConsumer(QnRtspConnectionProcessor* owner);
    ~QnRtspDataConsumer();

    void pauseNetwork();
    void resumeNetwork();

    void setLastSendTime(qint64 time);
    void setWaitCSeq(qint64 newTime, int sceq);
    virtual void putData(const QnAbstractDataPacketPtr& data);
    virtual bool canAcceptData() const;
    void setLiveMode(bool value);
    int copyLastGopFromCamera(bool usePrimaryStream, qint64 skipTime, quint32 cseq);
    void lockDataQueue();
    void unlockDataQueue();
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
protected:
    //QnMediaContextPtr getGeneratedContext(CodecID compressionType);
    virtual bool processData(const QnAbstractDataPacketPtr& data);

    void createDataPacketTCP(QnByteArray& sendBuffer, const QnAbstractMediaDataPtr& media, int rtpTcpChannel);

    // delay streaming. Used for realtime mode streaming
    void doRealtimeDelay(QnConstAbstractMediaDataPtr media);

    bool isMediaTimingsSlow() const;
    void setLiveQualityInternal(MediaQuality quality);
    qint64 dataQueueDuration();
    void sendMetadata(const QByteArray& metadata);
    void getEdgePackets(qint64& firstVTime, qint64& lastVTime, bool checkLQ) const;
private:
    //QMap<CodecID, QnMediaContextPtr> m_generatedContext;
    bool m_gotLivePacket;
    QByteArray m_codecCtxData;
    //QMap<int, QList<int> > m_ctxSended;
    QElapsedTimer m_timer;
    //quint16 m_sequence[MAX_RTP_CHANNELS];
    //qint64 m_firstRtpTime[MAX_RTP_CHANNELS];
    QnRtspConnectionProcessor* m_owner;
    qint64 m_lastSendTime;
    qint64 m_rtStartTime; // used for realtime streaming mode
    qint64 m_lastRtTime; // used for realtime streaming mode
    qint64 m_lastMediaTime; // same as m_lastSendTime, but show real timestamp for LIVE video (m_lastSendTime always returns DATETIME_NOW for live)
    //char m_rtpHeader[RtpHeader::RTP_HEADER_SIZE];
    QMutex m_mutex;
    QMutex m_qualityChangeMutex;
    int m_waitSCeq;
    bool m_liveMode;
    bool m_pauseNetwork;
    QMutex m_dataQueueMtx;
    bool m_singleShotMode;
    int m_packetSended;
    QnAbstractStreamDataProvider* m_prefferedProvider;
    QnAbstractStreamDataProvider* m_currentDP;
    int m_liveMarker;
    MediaQuality m_liveQuality;
    MediaQuality m_newLiveQuality;

    static QHash<QHostAddress, qint64> m_lastSwitchTime;
    static QSet<QnRtspDataConsumer*> m_allConsumers;
    static QMutex m_allConsumersMutex;
    int m_streamingSpeed;
    bool m_multiChannelVideo;
    QnAdaptiveSleep m_adaptiveSleep;
    bool m_useUTCTime; // use absolute UTC file for RTP (used for proprietary format)
    int m_fastChannelZappingSize;
    
    qint64 m_firstLiveTime;
    qint64 m_lastLiveTime;
    QElapsedTimer m_liveTimer;
    mutable QMutex m_liveTimingControlMtx;
    bool m_allowAdaptiveStreaming;

    QnByteArray m_sendBuffer;
    bool m_someDataIsDropped;
    qint64 m_previousRtpTimestamp;
    qint64 m_previousScaledRtpTimestamp;
};
#endif // __RTSP_DATA_CONSUMER_H__
