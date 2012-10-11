#include <QMap>
#include "rtsp_data_consumer.h"
#include "core/datapacket/media_data_packet.h"
#include "rtsp_connection.h"
#include "utils/common/util.h"
#include "utils/media/ffmpeg_helper.h"
#include "camera/video_camera.h"
#include "camera/camera_pool.h"
#include "utils/common/sleep.h"
#include "utils/network/rtpsession.h"
#include "core/dataprovider/abstract_streamdataprovider.h"
#include "utils/common/synctime.h"

static const int MAX_QUEUE_SIZE = 10;
//static const QString RTP_FFMPEG_GENERIC_STR("mpeg4-generic"); // this line for debugging purpose with VLC player

static const int MAX_RTSP_WRITE_BUFFER = 1024*1024;
static const int MAX_PACKETS_AT_SINGLE_SHOT = 3;
static const int HIGH_QUALITY_RETRY_COUNTER = 1;
static const int QUALITY_SWITCH_INTERVAL = 1000 * 5; // delay between high quality switching attempts
static const int MAX_CLIENT_BUFFER_SIZE_MS = 1000*2;

QHash<QHostAddress, qint64> QnRtspDataConsumer::m_lastSwitchTime;
QSet<QnRtspDataConsumer*> QnRtspDataConsumer::m_allConsumers;
QMutex QnRtspDataConsumer::m_allConsumersMutex;


QnRtspDataConsumer::QnRtspDataConsumer(QnRtspConnectionProcessor* owner):
  QnAbstractDataConsumer(MAX_QUEUE_SIZE),
  m_owner(owner),
  m_lastSendTime(0),
  m_rtStartTime(AV_NOPTS_VALUE),
  m_lastRtTime(0),
  m_lastMediaTime(0),
  m_waitSCeq(0),
  m_liveMode(false),
  m_pauseNetwork(false),
  m_singleShotMode(false),
  m_packetSended(false),
  m_prefferedProvider(0),
  m_currentDP(0),
  m_liveQuality(MEDIA_Quality_High),
  m_newLiveQuality(MEDIA_Quality_None),
  m_hiQualityRetryCounter(0),
  m_realtimeMode(false),
  m_adaptiveSleep(MAX_FRAME_DURATION*1000),
  m_useUTCTime(true),
  m_fastChannelZappingSize(0),
  m_firstLiveTime(AV_NOPTS_VALUE),
  m_lastLiveTime(AV_NOPTS_VALUE),
  m_sendBuffer(CL_MEDIA_ALIGNMENT, 1024*256),
  m_allowAdaptiveStreaming(true)
{
    m_timer.start();
    QMutexLocker lock(&m_allConsumersMutex);
    foreach(QnRtspDataConsumer* consumer, m_allConsumers)
    {
        if (m_owner->getPeerAddress() == consumer->m_owner->getPeerAddress())
        {
            if (consumer->m_liveQuality == MEDIA_Quality_Low) {
                m_liveQuality = MEDIA_Quality_Low;
                m_hiQualityRetryCounter = HIGH_QUALITY_RETRY_COUNTER;
                break;
            }
        }
    }

    m_allConsumers << this;
}

QnRtspDataConsumer::~QnRtspDataConsumer()
{
    {
        QMutexLocker lock(&m_allConsumersMutex);
        m_allConsumers.remove(this);
        if (m_firstLiveTime != AV_NOPTS_VALUE)
        {
            // If some data was transfered and camera is closing, some bandwidth appears.
            // Try to switch some camera(s) to high quality
            foreach(QnRtspDataConsumer* consumer, m_allConsumers)
            {
                if (m_owner->getPeerAddress() == consumer->m_owner->getPeerAddress())
                {
                    if (consumer->m_liveQuality == MEDIA_Quality_Low)
                    {
                        consumer->resetQualityStatistics();
                        if (m_liveQuality == MEDIA_Quality_Low)
                            break; // try only one camera is current quality is low
                    }
                }
            }
        }
    }
    stop();
}

  void QnRtspDataConsumer::pauseNetwork()
{
    m_pauseNetwork = true;
    m_fastChannelZappingSize = 0;
}
void QnRtspDataConsumer::resumeNetwork()
{
    m_pauseNetwork = false;
}

//qint64 lastSendTime() const { return m_lastSendTime; }
void QnRtspDataConsumer::setLastSendTime(qint64 time) 
{ 
    m_lastMediaTime = m_lastSendTime = time; 
}
void QnRtspDataConsumer::setWaitCSeq(qint64 newTime, int sceq)
{ 
    QMutexLocker lock(&m_mutex);
    m_waitSCeq = sceq; 
    m_lastMediaTime = m_lastSendTime = newTime;
}

qint64 QnRtspDataConsumer::getCurrentTime() const
{
    return m_lastSendTime; 
}

qint64 QnRtspDataConsumer::getNextTime() const
{
    return m_lastSendTime; 
}

qint64 QnRtspDataConsumer::getDisplayedTime() const 
{ 
    return m_lastSendTime; 
}

qint64 QnRtspDataConsumer::getExternalTime() const
{
    return m_lastSendTime; 
}

bool removeItemsCondition(const QnAbstractDataPacketPtr& data)
{
    return !(qSharedPointerDynamicCast<QnAbstractMediaData>(data)->flags & AV_PKT_FLAG_KEY);
}

void QnRtspDataConsumer::resetQualityStatistics()
{
    m_hiQualityRetryCounter = 0;
}

bool QnRtspDataConsumer::canSwitchToLowQuality()
{
    if (!m_owner->isSecondaryLiveDPSupported())
        return false;

    return true;

    QMutexLocker lock(&m_allConsumersMutex);
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    QHostAddress clientAddress = m_owner->getPeerAddress();
    if (currentTime - m_lastSwitchTime[clientAddress] < QUALITY_SWITCH_INTERVAL)
        return false;

    m_lastSwitchTime[clientAddress] = currentTime;
    return true;
}

bool QnRtspDataConsumer::canSwitchToHiQuality()
{
    // RTSP queue is almost empty. But need some addition check to prevent quality change flood
    QMutexLocker lock(&m_allConsumersMutex);
    if (m_hiQualityRetryCounter >= HIGH_QUALITY_RETRY_COUNTER)
        return false;

    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    QHostAddress clientAddress = m_owner->getPeerAddress();
    if (currentTime - m_lastSwitchTime[clientAddress] < QUALITY_SWITCH_INTERVAL)
        return false;

    m_lastSwitchTime[clientAddress] = currentTime;
    m_hiQualityRetryCounter++;
    return true;
}

bool QnRtspDataConsumer::isMediaTimingsSlow() const
{
    QMutexLocker lock(&m_liveTimingControlMtx);
    if (m_lastLiveTime == AV_NOPTS_VALUE)
        return false;
    Q_ASSERT(m_firstLiveTime != AV_NOPTS_VALUE);
    qint64 elapsed = m_liveTimer.elapsed()*1000;
    bool rez = m_lastLiveTime - m_firstLiveTime < m_liveTimer.elapsed()*1000;

    return rez;
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

    // quality control
    bool isLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;
    if (isLive)
    {
        bool isSecondaryProvider = m_owner->isSecondaryLiveDP(data->dataProvider);
        if (isSecondaryProvider)
            media->flags |= QnAbstractMediaData::MediaFlags_LowQuality;


        if (m_allowAdaptiveStreaming && m_newLiveQuality == MEDIA_Quality_None && m_liveQuality != MEDIA_Quality_AlwaysHigh)
        {
            //if (m_dataQueue.size() >= m_dataQueue.maxSize()-MAX_QUEUE_SIZE/4 && m_liveQuality == MEDIA_Quality_High  && canSwitchToLowQuality())
            if (m_dataQueue.size() >= m_dataQueue.maxSize()-MAX_QUEUE_SIZE/4 && m_liveQuality == MEDIA_Quality_High && canSwitchToLowQuality() && isMediaTimingsSlow())
                m_newLiveQuality = MEDIA_Quality_Low; // slow network. Reduce quality
            else if (m_dataQueue.size() <= 1 && m_liveQuality == MEDIA_Quality_Low && canSwitchToHiQuality()) 
                m_newLiveQuality = MEDIA_Quality_High;
        }
    }

    // overflow control
    if ((media->flags & AV_PKT_FLAG_KEY) && m_dataQueue.size() > m_dataQueue.maxSize())
    {
        m_dataQueue.lock();
        bool clearHiQ = false; // remove LQ packets, keep HQ
        if (m_liveQuality != MEDIA_Quality_AlwaysHigh) {
            m_newLiveQuality = MEDIA_Quality_Low;
            clearHiQ = true; // remove HQ packets, keep LQ
        }

        bool somethingDeleted = false;
        for (int i = m_dataQueue.size()-1; i >=0; --i)
        {
            QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData> (m_dataQueue.at(i));
            if (media->flags & AV_PKT_FLAG_KEY) 
            {
                bool isLQ = media->flags & QnAbstractMediaData::MediaFlags_LowQuality;
                if (isLQ && clearHiQ || !isLQ && !clearHiQ)
                {
                    m_dataQueue.removeFirst(i);
                    somethingDeleted = true;
                    break;
                }
            }
        }
        if (!somethingDeleted)
        {
            for (int i = m_dataQueue.size()-1; i >=0; --i)
            {
                QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData> (m_dataQueue.at(i));
                if (media->flags & AV_PKT_FLAG_KEY)
                {
                    m_dataQueue.removeFirst(i);
                    break;
                }
            }
        }
        m_dataQueue.unlock();
    }

    /*
    if ((media->flags & AV_PKT_FLAG_KEY) && m_dataQueue.size() > m_dataQueue.maxSize())
    {
        m_dataQueue.lock();

        m_newLiveQuality = MEDIA_Quality_Low;

        QnAbstractMediaDataPtr dataLow;
        QnAbstractMediaDataPtr dataHi;
        for (int i = m_dataQueue.size()-1; i >=0; --i)
        {
            QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData> (m_dataQueue.at(i));
            if (media->flags & AV_PKT_FLAG_KEY)
            {
                if (media->flags & QnAbstractMediaData::MediaFlags_LowQuality)
                {
                    if (dataLow == 0)
                        dataLow = media;
                }
                else {
                    if (dataHi == 0)
                        dataHi = media;
                }
            }
        }

        // some data need to remove
        int i = 0;
        while (dataHi || dataLow)
        {
            QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData> (m_dataQueue.at(i));
            bool deleted = false;
            bool isLow = media->flags & QnAbstractMediaData::MediaFlags_LowQuality;
            if (isLow && dataLow)
            {
                if (media == dataLow) {
                    // all data before dataLow removed
                    dataLow.clear(); 
                }
                else {
                    m_dataQueue.removeAt(i);
                    deleted = true;
                }
            }
            if (!isLow && dataHi)
            {
                if (media == dataHi) {
                    // all data before dataHi removed
                    dataHi.clear(); 
                }
                else {
                    m_dataQueue.removeAt(i);
                    deleted = true;
                }
            }
            if (!deleted)
                ++i;
        }
        m_dataQueue.unlock();
    }
    */

    while(m_dataQueue.size() > 120) // queue to large
    {
        QnAbstractDataPacketPtr tmp;
        m_dataQueue.pop(tmp);
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

void QnRtspDataConsumer::setLiveQuality(MediaQuality liveQuality)
{
    m_newLiveQuality = liveQuality;
}

void QnRtspDataConsumer::buildRTPHeader(char* buffer, quint32 ssrc, int markerBit, quint32 timestamp, quint8 payloadType, quint16 sequence)
{
    RtpHeader* rtp = (RtpHeader*) buffer;
    rtp->version = RtpHeader::RTP_VERSION;
    rtp->padding = 0;
    rtp->extension = 0;
    rtp->CSRCCount = 0;
    rtp->marker  =  markerBit;
    rtp->payloadType = payloadType;
    rtp->sequence = htons(sequence);
    //rtp->timestamp = htonl(m_timer.elapsed());
    rtp->timestamp = htonl(timestamp);
    rtp->ssrc = htonl(ssrc); // source ID
}

/*
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

void QnRtspDataConsumer::createDataPacketTCP(QnByteArray& sendBuffer, QnAbstractMediaDataPtr media, int rtpTcpChannel)
{
    quint16 flags = media->flags;
    int cseq = media->opaque;

    if (m_owner->isLiveDP(media->dataProvider)) {
        flags |= QnAbstractMediaData::MediaFlags_LIVE;
        cseq = m_liveMarker;
    }
    if (flags & QnAbstractMediaData::MediaFlags_AfterEOF)
        m_ctxSended.clear();

    bool isLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;
    if (isLive) {
        if (!m_gotLivePacket)
            flags |= QnAbstractMediaData::MediaFlags_BOF;
        m_gotLivePacket = true;
    }


    // one video channel may has several subchannels (video combined with frames from difference codecContext)
    // max amount of subchannels is MAX_CONTEXTS_AT_VIDEO. Each channel used 2 ssrc: for data and for CodecContext

    quint32 ssrc = BASIC_FFMPEG_SSRC + media->channelNumber * MAX_CONTEXTS_AT_VIDEO*2;

    ssrc += media->subChannelNumber*2;
    int subChannelNumber = media->subChannelNumber;

    QnMetaDataV1Ptr metadata = qSharedPointerDynamicCast<QnMetaDataV1>(media);

    if (!metadata && media->compressionType)
    {
        QList<QnMediaContextPtr>& ctxData = m_ctxSended[media->channelNumber];
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
            buildRtspTcpHeader(rtpTcpChannel, ssrc + 1, codecCtxData.size(), true, 0, RTP_FFMPEG_GENERIC_CODE); // ssrc+1 - switch data subchannel to context subchannel
            sendBuffer.write(m_rtspTcpHeader, rtpHeaderSize);
            Q_ASSERT(!codecCtxData.isEmpty());
            m_owner->bufferData(codecCtxData);
        }
    }

    // send data with RTP headers
    QnCompressedVideoData *video = media.dynamicCast<QnCompressedVideoData>().data();
    const char* curData = media->data.data();
    int sendLen = 0;
    int ffHeaderSize = RTSP_FFMPEG_GENERIC_HEADER_SIZE;
    if (video)
        ffHeaderSize += RTSP_FFMPEG_VIDEO_HEADER_SIZE;
    else if (metadata)
        ffHeaderSize += RTSP_FFMPEG_METADATA_HEADER_SIZE;

    if (m_lastSendTime != DATETIME_NOW)
        m_lastSendTime = media->timestamp;
    m_lastMediaTime = media->timestamp;

    m_mutex.unlock();

    for (int dataRest = media->data.size(); dataRest > 0 || ffHeaderSize; dataRest -= sendLen)
    {
        while (m_pauseNetwork && !m_needStop)
        {
            QnSleep::msleep(1);
        }

        sendLen = qMin(MAX_RTSP_DATA_LEN - ffHeaderSize, dataRest);
        buildRtspTcpHeader(rtpTcpChannel, ssrc, sendLen + ffHeaderSize, sendLen >= dataRest ? 1 : 0, media->timestamp, RTP_FFMPEG_GENERIC_CODE);
        //QMutexLocker lock(&m_owner->getSockMutex());
        m_owner->bufferData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
        if (ffHeaderSize) 
        {
            quint8 packetType = media->dataType;
            m_owner->bufferData((const char*) &packetType, 1);
            quint32 timestampHigh = htonl(media->timestamp >> 32);
            m_owner->bufferData((const char*) &timestampHigh, 4);
            quint8 cseq8 = cseq;
            m_owner->bufferData((const char*) &cseq8, 1);
            flags = htons(flags);
            m_owner->bufferData((const char*) &flags, 2);
            if (video) 
            {
                quint32 videoHeader = htonl(video->data.size() & 0x00ffffff);
                m_owner->bufferData(((const char*) &videoHeader)+1, 3);
            }
            else if (metadata) {
                quint32 metadataHeader = htonl(metadata->m_duration/1000);
                m_owner->bufferData((const char*) &metadataHeader, 4);
            }
            ffHeaderSize = 0;
        }

        if(sendLen > 0)
            m_owner->bufferData(curData, sendLen);
        curData += sendLen;

        m_owner->sendBuffer();
        m_owner->clearBuffer();
    }
}
*/

void QnRtspDataConsumer::setUseRealTimeStreamingMode(bool value)
{
    m_realtimeMode = value;
}


void QnRtspDataConsumer::doRealtimeDelay(QnAbstractMediaDataPtr media)
{
    if (m_rtStartTime == AV_NOPTS_VALUE) {
        m_rtStartTime = media->timestamp;
    }
    else {
        qint64 timeDiff = media->timestamp - m_lastRtTime;
        if (timeDiff <= MAX_FRAME_DURATION*1000)
            m_adaptiveSleep.terminatedSleep(timeDiff, MAX_FRAME_DURATION*1000); // if diff too large, it is recording hole. do not calc delay for this case
    }
    m_lastRtTime = media->timestamp;
}

bool QnRtspDataConsumer::processData(QnAbstractDataPacketPtr data)
{
    if (m_pauseNetwork)
        return false; // does not ready to process data. please wait

    //msleep(500);

    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!media)
        return true;

    QnMetaDataV1Ptr metadata = qSharedPointerDynamicCast<QnMetaDataV1>(data);

    if (metadata == 0)
    {
        bool isKeyFrame = media->flags & AV_PKT_FLAG_KEY;
        bool isSecondaryProvider = m_owner->isSecondaryLiveDP(media->dataProvider);
        if (isKeyFrame && m_newLiveQuality != MEDIA_Quality_None)
        {
            if (m_newLiveQuality == MEDIA_Quality_Low && isSecondaryProvider) {
                m_liveQuality = MEDIA_Quality_Low; // slow network. Reduce quality
                m_newLiveQuality = MEDIA_Quality_None;
            }
            else if ((m_newLiveQuality == MEDIA_Quality_High || m_newLiveQuality == MEDIA_Quality_AlwaysHigh) && !isSecondaryProvider) {
                m_liveQuality = m_newLiveQuality;
                m_newLiveQuality = MEDIA_Quality_None;
            }
        }

        if (m_liveQuality != MEDIA_Quality_Low && isSecondaryProvider)
            return true; // data for other live quality stream
        else if (m_liveQuality == MEDIA_Quality_Low && m_owner->isPrimaryLiveDP(media->dataProvider))
            return true; // data for other live quality stream
    }

    RtspServerTrackInfoPtr trackInfo = m_owner->getTrackInfo(media->channelNumber);
    if (trackInfo == 0 || trackInfo->encoder == 0 || trackInfo->clientPort == -1)
        return true; // skip data (for example audio is disabled)
    QnRtspEncoderPtr codecEncoder = trackInfo->encoder;
    {
        QMutexLocker lock(&m_mutex);
        int cseq = media->opaque;
        if (m_waitSCeq) {
            if (cseq != m_waitSCeq)
                return true; // ignore data
            m_waitSCeq = 0;
            m_rtStartTime = AV_NOPTS_VALUE;
            m_adaptiveSleep.afterdelay(); // same as reset call
            m_adaptiveSleep.addQuant(MAX_CLIENT_BUFFER_SIZE_MS); // allow to stream data faster at start streaming for some client prebuffer. value in ms

            m_owner->resetTrackTiming();
        }
        if (m_lastSendTime != DATETIME_NOW)
            m_lastSendTime = media->timestamp;
        m_lastMediaTime = media->timestamp;
    }

    bool isLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;
    if (m_realtimeMode && !isLive)
        doRealtimeDelay(media);

    if (isLive && media->dataType == QnAbstractMediaData::VIDEO) 
    {
        QMutexLocker lock(&m_liveTimingControlMtx);
        if (m_firstLiveTime == AV_NOPTS_VALUE) {
            m_liveTimer.restart();
            m_lastLiveTime = m_firstLiveTime = media->timestamp;
        }
    }

    QnRtspFfmpegEncoderPtr ffmpegEncoder = qSharedPointerDynamicCast<QnRtspFfmpegEncoder>(codecEncoder);
    if (ffmpegEncoder)
    {
        ffmpegEncoder->setAdditionFlags(0);
        if (isLive) {
            ffmpegEncoder->setLiveMarker(m_liveMarker);
            if (m_fastChannelZappingSize > 0) {
                ffmpegEncoder->setAdditionFlags(QnAbstractMediaData::MediaFlags_FCZ);
                m_fastChannelZappingSize--;
            }
        }
    }

    codecEncoder->setDataPacket(media);
    if (trackInfo->firstRtpTime == -1)
        trackInfo->firstRtpTime = media->timestamp;
    static AVRational r = {1, 1000000};
    AVRational time_base = {1, codecEncoder->getFrequency() };

    /*
    qint64 timeDiff = media->timestamp;
    if (!m_useUTCTime)
        timeDiff -= trackInfo->firstRtpTime; // enumerate RTP time from 0 after seek
    qint64 packetTime = av_rescale_q(timeDiff, r, time_base);
    */
    qint64 packetTime = av_rescale_q(media->timestamp, r, time_base);

    m_sendBuffer.resize(4); // reserve space for RTP TCP header
    while(!m_needStop && codecEncoder->getNextPacket(m_sendBuffer))
    {
        while (m_pauseNetwork && !m_needStop)
            QnSleep::msleep(1);

        bool isRtcp = false;
        if (codecEncoder->isRtpHeaderExists()) 
        {
            RtpHeader* packet = (RtpHeader*) (m_sendBuffer.data() + 4);
            isRtcp = packet->payloadType >= 72 && packet->payloadType <= 76;
        }
        else {
            buildRTPHeader(m_sendBuffer.data() + 4, codecEncoder->getSSRC(), codecEncoder->getRtpMarker(), packetTime, codecEncoder->getPayloadtype(), trackInfo->sequence++); 
        }
        
        if (m_owner->isTcpMode()) {
            m_sendBuffer.data()[0] = '$';
            m_sendBuffer.data()[1] = isRtcp ? trackInfo->clientRtcpPort : trackInfo->clientPort;
            quint16* lenPtr = (quint16*) (m_sendBuffer.data() + 2);
            *lenPtr = htons(m_sendBuffer.size() - 4);
            m_owner->sendBuffer(m_sendBuffer);
        }
        else {
            Q_ASSERT(m_sendBuffer.size() > 4 && m_sendBuffer.size() < 16384);
            UDPSocket* mediaSocket = isRtcp ? trackInfo->rtcpSocket : trackInfo->mediaSocket;
            mediaSocket->sendTo(m_sendBuffer.data()+4, m_sendBuffer.size()-4);
        }

        m_sendBuffer.resize(4); // reserve space for RTP TCP header
    }
    m_sendBuffer.clear();


    //m_owner->sendCurrentRangeIfUpdated();
    QByteArray newRange = m_owner->getRangeHeaderIfChanged().toUtf8();
    if (!newRange.isEmpty())
    {
        int sendLen = newRange.size();
        trackInfo = m_owner->getTrackInfo(m_owner->getMetadataChannelNum());
        if (trackInfo) 
        {
            m_sendBuffer.resize(16);
            buildRTPHeader(m_sendBuffer.data()+4, METADATA_SSRC, newRange.size(), qnSyncTime->currentMSecsSinceEpoch(), RTP_METADATA_CODE, trackInfo->sequence);
            m_sendBuffer.write(newRange);

            if (m_owner->isTcpMode()) {
                m_sendBuffer.data()[0] = '$';
                m_sendBuffer.data()[1] = trackInfo->clientPort;
                quint16* lenPtr = (quint16*) (m_sendBuffer.data() + 2);
                *lenPtr = htons(m_sendBuffer.size() - 4);
                m_owner->sendBuffer(m_sendBuffer);
            }
            else  if (trackInfo->mediaSocket) {
                Q_ASSERT(m_sendBuffer.size() > 4 && m_sendBuffer.size() < 16384);
                trackInfo->mediaSocket->sendTo(m_sendBuffer.data()+4, m_sendBuffer.size()-4);
            }

            trackInfo->sequence++;
            m_sendBuffer.clear();
        }
    }

    if (m_packetSended++ == MAX_PACKETS_AT_SINGLE_SHOT)
        m_singleShotMode = false;

    QMutexLocker lock(&m_liveTimingControlMtx);
    if (media->dataType == QnAbstractMediaData::VIDEO && m_lastLiveTime != AV_NOPTS_VALUE)
        m_lastLiveTime = media->timestamp;

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

void QnRtspDataConsumer::addData(QnAbstractMediaDataPtr data)
{
    m_dataQueue.push(data);
}

int QnRtspDataConsumer::copyLastGopFromCamera(bool usePrimaryStream, qint64 skipTime)
{
    // Fast channel zapping
    int prevSize = m_dataQueue.size();
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(m_owner->getResource());
    int copySize = 0;
    if (camera)
        copySize = camera->copyLastGop(usePrimaryStream, skipTime, m_dataQueue);
    m_dataQueue.setMaxSize(m_dataQueue.size()-prevSize + MAX_QUEUE_SIZE);
    m_fastChannelZappingSize = copySize;

    QMutexLocker lock(&m_liveTimingControlMtx);
    m_firstLiveTime = AV_NOPTS_VALUE;
    m_lastLiveTime = AV_NOPTS_VALUE;

    return copySize;
}

void QnRtspDataConsumer::setSingleShotMode(bool value)
{
    m_singleShotMode = value;
    m_packetSended = 0;
}

qint64 QnRtspDataConsumer::lastQueuedTime()
{
    if (m_dataQueue.size() == 0)
        return m_lastMediaTime;
    else {
        QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData> (m_dataQueue.last());
        if (media)
            return media->timestamp;
        else
            return m_lastMediaTime;
    }
}

void QnRtspDataConsumer::setLiveMarker(int marker)
{
    m_liveMarker = marker;
}

void QnRtspDataConsumer::clearUnprocessedData()
{
    QnAbstractDataConsumer::clearUnprocessedData();
    m_newLiveQuality = MEDIA_Quality_None;
    m_dataQueue.setMaxSize(MAX_QUEUE_SIZE);
    m_hiQualityRetryCounter = 0;
}

void QnRtspDataConsumer::setUseUTCTime(bool value)
{
    m_useUTCTime = value;
}

void QnRtspDataConsumer::setAllowAdaptiveStreaming(bool value)
{
    m_allowAdaptiveStreaming = value;
}
