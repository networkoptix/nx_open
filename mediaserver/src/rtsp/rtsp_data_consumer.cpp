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
  m_fastChannelZappingSize(0)
{
    memset(m_sequence, 0, sizeof(m_sequence));
    for (int i = 0; i < MAX_RTP_CHANNELS; ++i)
        m_firstRtpTime[i] = -1;

    m_timer.start();
    QMutexLocker lock(&m_allConsumersMutex);
    m_allConsumers << this;
}

QnRtspDataConsumer::~QnRtspDataConsumer()
{
    {
        QMutexLocker lock(&m_allConsumersMutex);
        m_allConsumers.remove(this);
        foreach(QnRtspDataConsumer* consumer, m_allConsumers)
        {
            if (m_owner->getPeerAddress() == consumer->m_owner->getPeerAddress())
                consumer->resetQualityStatistics();
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

        if (m_newLiveQuality == MEDIA_Quality_None)
        {
            if (m_dataQueue.size() >= m_dataQueue.maxSize()-MAX_QUEUE_SIZE/4 && m_liveQuality == MEDIA_Quality_High  && canSwitchToLowQuality())
                m_newLiveQuality = MEDIA_Quality_Low; // slow network. Reduce quality
            else if (m_dataQueue.size() <= 1 && m_liveQuality == MEDIA_Quality_Low && canSwitchToHiQuality()) 
                m_newLiveQuality = MEDIA_Quality_High;
        }
        
        bool isKeyFrame = media->flags & AV_PKT_FLAG_KEY;
        if (isKeyFrame && m_newLiveQuality != MEDIA_Quality_None)
        {
            if (m_newLiveQuality == MEDIA_Quality_Low && isSecondaryProvider) {
                m_liveQuality = MEDIA_Quality_Low; // slow network. Reduce quality
                m_newLiveQuality = MEDIA_Quality_None;
            }
            else if (m_newLiveQuality == MEDIA_Quality_High && !isSecondaryProvider) {
                m_liveQuality = MEDIA_Quality_High;
                m_newLiveQuality = MEDIA_Quality_None;
            }
        }
    }

    // overflow control
    if ((media->flags & AV_PKT_FLAG_KEY) && m_dataQueue.size() > m_dataQueue.maxSize())
    {
        m_dataQueue.lock();
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
    m_liveQuality = liveQuality;
}

void QnRtspDataConsumer::buildRtspTcpHeader(quint8 channelNum, quint32 ssrc, quint16 len, int markerBit, quint32 timestamp, quint8 payloadType)
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
    rtp->payloadType = payloadType;
    rtp->sequence = htons(m_sequence[channelNum]);
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
            m_adaptiveSleep.terminatedSleep(timeDiff, MAX_FRAME_DURATION); // if diff too large, it is recording hole. do not calc delay for this case
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
        if (m_liveQuality == MEDIA_Quality_High && m_owner->isSecondaryLiveDP(media->dataProvider))
            return true; // data for other live quality stream
        else if (m_liveQuality == MEDIA_Quality_Low && m_owner->isPrimaryLiveDP(media->dataProvider))
            return true; // data for other live quality stream
    }


    int rtpTcpChannel = m_owner->getAVTcpChannel(media->channelNumber);
    if (rtpTcpChannel == -1) 
        return true; // skip data (for example audio is disabled)
    QnRtspEncoderPtr codecEncoder = m_owner->getCodecEncoder(media->channelNumber);
    if (codecEncoder == 0)
        return true; // skip data (for example audio is disabled)

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

            memset(m_sequence, 0, sizeof(m_sequence));
            for (int i = 0; i < MAX_RTP_CHANNELS; ++i)
                m_firstRtpTime[i] = -1;
        }
        if (m_lastSendTime != DATETIME_NOW)
            m_lastSendTime = media->timestamp;
        m_lastMediaTime = media->timestamp;
    }
    if (m_realtimeMode)
        doRealtimeDelay(media);


    bool isLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;
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
    bool dataExists = true;
    if (m_firstRtpTime[media->channelNumber] == -1)
        m_firstRtpTime[media->channelNumber] = media->timestamp;
    static AVRational r = {1, 1000000};
    AVRational time_base = {1, codecEncoder->getFrequency() };
    qint64 timeDiff = media->timestamp;
    if (!m_useUTCTime)
        timeDiff -= m_firstRtpTime[media->channelNumber]; // enumerate RTP time from 0 after seek
    qint64 packetTime = av_rescale_q(timeDiff, r, time_base);


    while(dataExists && !m_needStop) 
    {
        while (m_pauseNetwork && !m_needStop)
            QnSleep::msleep(1);

        m_owner->clearBuffer();

        buildRtspTcpHeader(rtpTcpChannel, codecEncoder->getSSRC(), 0, codecEncoder->getRtpMarker(), packetTime, codecEncoder->getPayloadtype()); 
        m_owner->bufferData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
        dataExists = codecEncoder->getNextPacket(m_owner->getSendBuffer());
        if (dataExists) 
        {
            Q_ASSERT(m_owner->getSendBuffer().size() > 16);
            quint16* lenPtr = (quint16*) (m_owner->getSendBuffer().data() + 2);
            *lenPtr = htons(m_owner->getSendBuffer().size() - 4);
            m_owner->sendBuffer();
            m_sequence[rtpTcpChannel]++;
        }
    }
    m_owner->clearBuffer();


    //m_owner->sendCurrentRangeIfUpdated();
    QByteArray newRange = m_owner->getRangeHeaderIfChanged().toUtf8();
    if (!newRange.isEmpty())
    {
        int sendLen = newRange.size();
        int metadataTcpChannel = m_owner->getMetadataTcpChannel();
        if (metadataTcpChannel >= 0) {
            buildRtspTcpHeader(metadataTcpChannel, METADATA_SSRC, sendLen, 0, qnSyncTime->currentMSecsSinceEpoch(), RTP_METADATA_CODE);
            m_owner->bufferData(m_rtspTcpHeader, sizeof(m_rtspTcpHeader));
            m_owner->bufferData(newRange);
            m_owner->sendBuffer();
            m_sequence[metadataTcpChannel]++;
            m_owner->clearBuffer();
        }
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
