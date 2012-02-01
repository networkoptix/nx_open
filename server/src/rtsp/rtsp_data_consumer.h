#ifndef __RTSP_DATA_CONSUMER_H__
#define __RTSP_DATA_CONSUMER_H__

#include <QTime>
#include "core/dataconsumer/dataconsumer.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/datapacket/datapacket.h"
#include "utils/network/rtpsession.h"
#include "utils/media/externaltimesource.h"

class QnRtspConnectionProcessor;

static const int CLOCK_FREQUENCY = 1000000;
static const quint8 RTP_FFMPEG_GENERIC_CODE = 102;
static const QString RTP_FFMPEG_GENERIC_STR("FFMPEG");
static const int RTSP_MIN_SEEK_INTERVAL = 1000 * 30; // 30 ms as min seek interval

class QnRtspDataConsumer: public QnAbstractDataConsumer, public QnlTimeSource
{
public:
    QnRtspDataConsumer(QnRtspConnectionProcessor* owner);
    ~QnRtspDataConsumer();

    void pauseNetwork();
    void resumeNetwork();

    void setLastSendTime(qint64 time);
    void setWaitCSeq(qint64 newTime, int sceq);
    virtual void putData(QnAbstractDataPacketPtr data);
    virtual bool canAcceptData() const;
    void setLiveMode(bool value);
    void copyLastGopFromCamera(bool usePrimaryStream, qint64 skipTime = 0);
    void lockDataQueue();
    void unlockDataQueue();
    void setSingleShotMode(bool value);

    virtual qint64 getDisplayedTime() const;
    virtual qint64 getCurrentTime() const;
    virtual qint64 getNextTime() const;
    virtual qint64 getExternalTime() const;

    qint64 lastQueuedTime();
protected:
    void buildRtspTcpHeader(quint8 channelNum, quint32 ssrc, quint16 len, int markerBit, quint32 timestamp);
    QnMediaContextPtr getGeneratedContext(CodecID compressionType);
    virtual bool processData(QnAbstractDataPacketPtr data);
private:
    QMap<CodecID, QnMediaContextPtr> m_generatedContext;
    bool m_gotLivePacket;
    QByteArray m_codecCtxData;
    QMap<int, QList<QnMediaContextPtr> > m_ctxSended;
    //QMap<int, QList<int> > m_ctxSended;
    QTime m_timer;
    quint16 m_sequence[256];
    QnRtspConnectionProcessor* m_owner;
    qint64 m_lastSendTime;
    qint64 m_lastMediaTime; // same as m_lastSendTime, but show real timestamp for LIVE video (m_lastSendTime always returns DATETIME_NOW for live)
    char m_rtspTcpHeader[4 + RtpHeader::RTP_HEADER_SIZE];
    quint8* tcpReadBuffer;
    QMutex m_mutex;
    int m_waitSCeq;
    bool m_liveMode;
    bool m_pauseNetwork;
    QMutex m_dataQueueMtx;
    bool m_singleShotMode;
    int m_packetSended;
    QnAbstractStreamDataProvider* m_prefferedProvider;
    QnAbstractStreamDataProvider* m_currentDP;
};

#endif // __RTSP_DATA_CONSUMER_H__
