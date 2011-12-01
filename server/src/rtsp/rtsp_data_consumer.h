#ifndef __RTSP_DATA_CONSUMER_H__
#define __RTSP_DATA_CONSUMER_H__

#include <QTime>
#include "core/dataconsumer/dataconsumer.h"
#include "utils/network/rtp_stream_parser.h"
#include "core/datapacket/datapacket.h"
#include "utils/network/rtpsession.h"

class QnRtspConnectionProcessor;

static const int CLOCK_FREQUENCY = 1000000;
static const quint8 RTP_FFMPEG_GENERIC_CODE = 102;
static const QString RTP_FFMPEG_GENERIC_STR("FFMPEG");
static const int RTSP_MIN_SEEK_INTERVAL = 1000 * 30; // 30 ms as min seek interval

class QnRtspDataConsumer: public QnAbstractDataConsumer
{
public:
    QnRtspDataConsumer(QnRtspConnectionProcessor* owner);

    void pauseNetwork();
    void resumeNetwork();

    void setLastSendTime(qint64 time);
    void setWaitCSeq(qint64 newTime, int sceq);
    virtual qint64 getCurrentTime() const;
    virtual void putData(QnAbstractDataPacketPtr data);
    virtual bool canAcceptData() const;
    void setLiveMode(bool value);
    void copyLastGopFromCamera();
    void lockDataQueue();
    void unlockDataQueue();
    void setSingleShotMode(bool value);
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
    char m_rtspTcpHeader[4 + RtpHeader::RTP_HEADER_SIZE];
    quint8* tcpReadBuffer;
    QMutex m_mutex;
    int m_waitSCeq;
    bool m_liveMode;
    bool m_pauseNetwork;
    QMutex m_dataQueueMtx;
    bool m_singleShotMode;
    int m_packetSended;
};

#endif // __RTSP_DATA_CONSUMER_H__
