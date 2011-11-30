#include <QMap>
#include "rtsp_data_consumer.h"
#include "core/datapacket/mediadatapacket.h"
#include "rtsp_connection.h"
#include "utils/common/util.h"
#include "utils/media/ffmpeg_helper.h"
#include "camera/video_camera.h"
#include "camera/camera_pool.h"
#include "utils/common/sleep.h"
#include "utils/network/rtpsession.h"

static const int MAX_QUEUE_SIZE = 15;
//static const QString RTP_FFMPEG_GENERIC_STR("mpeg4-generic"); // this line for debugging purpose with VLC player

static const int MAX_RTSP_WRITE_BUFFER = 1024*1024;
static const int MAX_PACKETS_AT_SINGLE_SHOT = 3;


QnRtspDataConsumer::QnRtspDataConsumer(QnRtspConnectionProcessor* owner):
  QnAbstractDataConsumer(MAX_QUEUE_SIZE),
  m_owner(owner),
  m_lastSendTime(0),
  m_waitSCeq(0),
  m_liveMode(false),
  m_pauseNetwork(false),
  m_gotLivePacket(false),
  m_singleShotMode(false),
  m_packetSended(false)
{
    memset(m_sequence, 0, sizeof(m_sequence));
    m_timer.start();
}

  void QnRtspDataConsumer::pauseNetwork()
{
    m_pauseNetwork = true;
}
void QnRtspDataConsumer::resumeNetwork()
{
    m_pauseNetwork = false;
}

//qint64 lastSendTime() const { return m_lastSendTime; }
void QnRtspDataConsumer::setLastSendTime(qint64 time) 
{ 
    m_lastSendTime = time; 
}
void QnRtspDataConsumer::setWaitCSeq(qint64 newTime, int sceq)
{ 
    QMutexLocker lock(&m_mutex);
    m_waitSCeq = sceq; 
    m_lastSendTime = newTime;
    m_ctxSended.clear();
    m_gotLivePacket = false;
}

qint64 QnRtspDataConsumer::getCurrentTime() const 
{ 
    return m_lastSendTime; 
}

bool removeItemsCondition(const QnAbstractDataPacketPtr& data)
{
    return !(qSharedPointerDynamicCast<QnAbstractMediaData>(data)->flags & AV_PKT_FLAG_KEY);
}
void QnRtspDataConsumer::putData(QnAbstractDataPacketPtr data)
{
//    cl_log.log("queueSize=", m_dataQueue.size(), cl_logALWAYS);
//    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
//    cl_log.log(QDateTime::fromMSecsSinceEpoch(media->timestamp/1000).toString("hh.mm.ss.zzz"), cl_logALWAYS);

    QMutexLocker lock(&m_dataQueueMtx);
    m_dataQueue.push(data);
    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    //if (m_dataQueue.size() > m_dataQueue.maxSize()*1.5) // additional space for archiveData (when archive->live switch occured, archive ordinary using all dataQueue size)
    if ((media->flags & AV_PKT_FLAG_KEY) && m_dataQueue.size() > m_dataQueue.maxSize() || m_dataQueue.size() > 100)
    {
        QnAbstractDataPacketPtr tmp;
        m_dataQueue.pop(tmp);
        m_dataQueue.removeFrontByCondition(removeItemsCondition);
    }
}

bool QnRtspDataConsumer::canAcceptData() const
{
    if (m_liveMode)
        return true;
    else if (m_singleShotMode)
        return m_dataQueue.size() == 0;
    else 
        return QnAbstractDataConsumer::canAcceptData();
}

void QnRtspDataConsumer::setLiveMode(bool value)
{
    m_liveMode = value;
}

void QnRtspDataConsumer::buildRtspTcpHeader(quint8 channelNum, quint32 ssrc, quint16 len, int markerBit, quint32 timestamp)
{
    m_rtspTcpHeader[0] = '$';
    m_rtspTcpHeader[1] = channelNum;
    quint16* lenPtr = (quint16*) &m_rtspTcpHeader[2];
    *lenPtr = htons(len+sizeof(RtpHeader));
    RtpHeader* rtp = (RtpHeader*) &m_rtspTcpHeader[4];
    rtp->version = RtpHeader::RTP_VERSION;
    rtp->padding = 0;
    rtp->extension = 0;
    rtp->CSRCCount = 0;
    rtp->marker  =  markerBit;
    rtp->payloadType = RTP_FFMPEG_GENERIC_CODE;
    rtp->sequence = htons(m_sequence[channelNum]++);
    //rtp->timestamp = htonl(m_timer.elapsed());
    rtp->timestamp = htonl(timestamp);
    rtp->ssrc = htonl(ssrc); // source ID
}

QnMediaContextPtr QnRtspDataConsumer::getGeneratedContext(CodecID compressionType)
{
    QMap<CodecID, QnMediaContextPtr>::iterator itr = m_generatedContext.find(compressionType);
    if (itr != m_generatedContext.end())
        return itr.value();
    QnMediaContextPtr result(new QnMediaContext(compressionType));
    AVCodecContext* ctx = result->ctx();
    m_generatedContext.insert(compressionType, result);
    return result;
}

bool QnRtspDataConsumer::processData(QnAbstractDataPacketPtr data)
{
    if (m_pauseNetwork)
        return false; // does not ready to process data. please wait

    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!media)
        return true;

    if (media->flags & QnAbstractMediaData::MediaFlags_AfterEOF)
    {
        m_dataQueue.clear();
        m_owner->switchToLive(); // it is archive EOF
        return true;
    }

    if (m_owner->isLiveDP(media->dataProvider)) {
        media->flags |= QnAbstractMediaData::MediaFlags_LIVE;
        if (!m_gotLivePacket)
            media->flags |= QnAbstractMediaData::MediaFlags_BOF;
        m_gotLivePacket = true;
    }

    int rtspChannelNum = media->channelNumber;
    if (media->dataType == QnAbstractMediaData::AUDIO)
        rtspChannelNum += m_owner->numOfVideoChannels();

    //QMutexLocker lock(&m_mutex);
    m_mutex.lock();
    if (media->flags & QnAbstractMediaData::MediaFlags_AfterEOF)
        m_ctxSended.clear();

    //if (m_waitBOF && !(media->flags & QnAbstractMediaData::MediaFlags_BOF))
    if (m_waitSCeq && media->opaque != m_waitSCeq)
    {
        m_mutex.unlock();
        return true; // ignore data
    }
    m_waitSCeq = 0;


    //if (!ctx)
    //    return true;

    // one video channel may has several subchannels (video combined with frames from difference codecContext)
    // max amount of subchannels is MAX_CONTEXTS_AT_VIDEO. Each channel used 2 ssrc: for data and for CodecContext
    
    quint32 ssrc = BASIC_FFMPEG_SSRC + rtspChannelNum * MAX_CONTEXTS_AT_VIDEO*2;

    ssrc += media->subChannelNumber*2;
    int subChannelNumber = media->subChannelNumber;

    QList<QnMediaContextPtr>& ctxData = m_ctxSended[rtspChannelNum];
    while (ctxData.size() <= subChannelNumber)
        ctxData << QnMediaContextPtr(0);

    QnMediaContextPtr currentContext = media->context;
    if (currentContext == 0)
        currentContext = getGeneratedContext(media->compressionType);
    int rtpHeaderSize = 4 + RtpHeader::RTP_HEADER_SIZE;
    if (ctxData[subChannelNumber] == 0 || !ctxData[subChannelNumber]->equalTo(currentContext.data()))
    {
        ctxData[subChannelNumber] = currentContext;
        QByteArray codecCtxData;
        QnFfmpegHelper::serializeCodecContext(currentContext->ctx(), &codecCtxData);
        buildRtspTcpHeader(rtspChannelNum, ssrc + 1, codecCtxData.size(), true, 0); // ssrc+1 - switch data subchannel to context subchannel
        QMutexLocker lock(&m_owner->getSockMutex());
        m_owner->sendData(m_rtspTcpHeader, rtpHeaderSize);
        Q_ASSERT(!codecCtxData.isEmpty());
        m_owner->sendData(codecCtxData);
    }

    // send data with RTP headers
    QnCompressedVideoData *video = media.dynamicCast<QnCompressedVideoData>().data();
    const char* curData = media->data.data();
    int sendLen = 0;
    int ffHeaderSize = 4 + (video ? RTSP_FFMPEG_VIDEO_HEADER_SIZE : 0);

    if (m_lastSendTime != DATETIME_NOW)
        m_lastSendTime = media->timestamp;

    m_mutex.unlock();

    for (int dataRest = media->data.size(); dataRest > 0; dataRest -= sendLen)
    {
        while (m_pauseNetwork && !m_needStop)
        {
            QnSleep::msleep(1);
        }

        sendLen = qMin(MAX_RTSP_DATA_LEN - ffHeaderSize, dataRest);
        buildRtspTcpHeader(rtspChannelNum, ssrc, sendLen + ffHeaderSize, sendLen >= dataRest ? 1 : 0, media->timestamp);
        QMutexLocker lock(&m_owner->getSockMutex());
        //m_owner->sendData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
        if (ffHeaderSize) 
        {
            quint8* hdrPointer = (quint8*) m_rtspTcpHeader + rtpHeaderSize;
            quint32 timestampHigh = htonl(media->timestamp >> 32);
            //m_owner->sendData((const char*) &timestampHigh, 4);
            memcpy(hdrPointer, &timestampHigh, 4);
            hdrPointer += 4;
            if (video) 
            {
                quint32 videoHeader = htonl((video->flags << 24) + (video->data.size() & 0x00ffffff));
                //m_owner->sendData((const char*) &videoHeader, 4);
                memcpy(hdrPointer, &videoHeader, 4);
                hdrPointer += 4;

                quint8 cseq = media->opaque;
                //m_owner->sendData((const char*) &cseq, 1);
                memcpy(hdrPointer, &cseq, 1);
            }
        }
        m_owner->sendData(m_rtspTcpHeader, rtpHeaderSize + ffHeaderSize);
        ffHeaderSize = 0;


        Q_ASSERT(sendLen > 0);
        m_owner->sendData(curData, sendLen);
        curData += sendLen;

    }
    if (m_packetSended++ == MAX_PACKETS_AT_SINGLE_SHOT)
        m_singleShotMode = false;
    return true;
}

void QnRtspDataConsumer::lockDataQueue()
{
    m_dataQueueMtx.lock();
}

void QnRtspDataConsumer::unlockDataQueue()
{
    m_dataQueueMtx.unlock();
}

void QnRtspDataConsumer::copyLastGopFromCamera()
{
    // Fast channel zapping
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(m_owner->getResource());
    if (camera)
    {
        camera->copyLastGop(m_dataQueue);
    }
}

void QnRtspDataConsumer::setSingleShotMode(bool value)
{
    m_singleShotMode = value;
    m_packetSended = 0;
}
