#include <QtCore/QMap>

#include "rtsp_data_consumer.h"

#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/config.h>
#include <rtsp/rtsp_connection.h>
#include <utils/common/util.h>
#include <utils/media/ffmpeg_helper.h>
#include <camera/video_camera.h>
#include <camera/camera_pool.h>
#include <utils/common/sleep.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <utils/common/synctime.h>
#include <core/resource/security_cam_resource.h>
#include <recorder/recording_manager.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/utils/scope_guard.h>

namespace {

bool needSecondaryStream(MediaQuality q)
{
    return isLowMediaQuality(q);
}

static const int kTcpSendBlockSize = 1024 * 16;
static const int kRtpTcpHeaderSize = 4;

} // namespace

const auto checkConstantsEquality = []()
{
    NX_ASSERT((AV_NOPTS_VALUE == DATETIME_INVALID) && "DATETIME_INVALID must be equal to AV_NOPTS_VALUE.");
    return true;
}();

static const int MAX_QUEUE_SIZE = 12;
static const qint64 TO_LOWQ_SWITCH_MIN_QUEUE_DURATION = 2000ll * 1000ll; // 2 seconds
//static const QString RTP_FFMPEG_GENERIC_STR("mpeg4-generic"); // this line for debugging purpose with VLC player

//static const int MAX_RTSP_WRITE_BUFFER = 1024*1024;
static const int MAX_PACKETS_AT_SINGLE_SHOT = 3;
//static const int HIGH_QUALITY_RETRY_COUNTER = 1;
//static const int QUALITY_SWITCH_INTERVAL = 1000 * 5; // delay between high quality switching attempts
static const int MAX_CLIENT_BUFFER_SIZE_MS = 1000*2;

QnRtspDataConsumer::QnRtspDataConsumer(QnRtspConnectionProcessor* owner):
    QnAbstractDataConsumer(MAX_QUEUE_SIZE),
    m_owner(owner),
    m_lastSendTime(0),
    m_rtStartTime(AV_NOPTS_VALUE),
    m_lastRtTime(0),
    m_lastMediaTime(0),
    m_waitSCeq(-1),
    m_liveMode(false),
    m_pauseNetwork(false),
    m_singleShotMode(false),
    m_packetSended(false),
    m_prefferedProvider(0),
    m_currentDP(0),
    m_liveQuality(MEDIA_Quality_High),
    m_newLiveQuality(MEDIA_Quality_None),
    m_streamingSpeed(MAX_STREAMING_SPEED),
    m_multiChannelVideo(false),
    m_adaptiveSleep(MAX_FRAME_DURATION_MS*1000),
    m_useUTCTime(true),
    m_fastChannelZappingSize(0),
    m_firstLiveTime(AV_NOPTS_VALUE),
    m_lastLiveTime(AV_NOPTS_VALUE),
    m_allowAdaptiveStreaming(true),
    m_sendBuffer(CL_MEDIA_ALIGNMENT, 1024*256),
    m_someDataIsDropped(false),
    m_previousRtpTimestamp(-1),
    m_previousScaledRtpTimestamp(-1),
    m_framesSinceRangeCheck(0),
    m_prevStartTime(AV_NOPTS_VALUE),
    m_prevEndTime(AV_NOPTS_VALUE),
    m_videoChannels(1)
{
    m_timer.start();
    m_needKeyData.fill(false);
}

void QnRtspDataConsumer::setResource(const QnResourcePtr& resource)
{
    if (!resource)
        return;
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;
    auto videoLayout = camera->getVideoLayout();
    if (!videoLayout)
        return;
    m_videoChannels = (quint32) videoLayout->channelCount();
}

QnRtspDataConsumer::~QnRtspDataConsumer()
{
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
    QnMutexLocker lock( &m_mutex );
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
    return !(std::dynamic_pointer_cast<QnAbstractMediaData>(data)->flags & AV_PKT_FLAG_KEY);
}

bool QnRtspDataConsumer::isMediaTimingsSlow() const
{
    QnMutexLocker lock( &m_liveTimingControlMtx );
    if (m_lastLiveTime == (qint64)AV_NOPTS_VALUE)
        return false;
    NX_ASSERT(m_firstLiveTime != (qint64)AV_NOPTS_VALUE);
    //qint64 elapsed = m_liveTimer.elapsed()*1000;
    bool rez = m_lastLiveTime - m_firstLiveTime < m_liveTimer.elapsed()*1000;

    return rez;
}

void QnRtspDataConsumer::getEdgePackets(
    const QnDataPacketQueue::RandomAccess& unsafeQueue,
    qint64& firstVTime,
    qint64& lastVTime,
    bool checkLQ) const
{
    for (int i = 0; i < unsafeQueue.size(); ++i)
    {
        const QnConstCompressedVideoDataPtr& video = std::dynamic_pointer_cast<const QnCompressedVideoData>(unsafeQueue.at(i));
        if (video && video->isLQ() == checkLQ) {
            firstVTime = video->timestamp;
            break;
        }
    }

    for (int i = unsafeQueue.size()-1; i >=0; --i)
    {
        const QnConstCompressedVideoDataPtr& video = std::dynamic_pointer_cast<const QnCompressedVideoData>(unsafeQueue.at(i));
        if (video && video->isLQ() == checkLQ) {
            lastVTime = video->timestamp;
            break;
        }
    }
}

qint64 QnRtspDataConsumer::dataQueueDuration()
{
    qint64 firstVTime = AV_NOPTS_VALUE;
    qint64 lastVTime = AV_NOPTS_VALUE;

    {
        auto unsafeQueue = m_dataQueue.lock();
        getEdgePackets(unsafeQueue, firstVTime, lastVTime, false);
        if (firstVTime == (qint64)AV_NOPTS_VALUE || lastVTime == (qint64)AV_NOPTS_VALUE)
            getEdgePackets(unsafeQueue, firstVTime, lastVTime, true);
    }

    if (firstVTime != (qint64)AV_NOPTS_VALUE && lastVTime != (qint64)AV_NOPTS_VALUE)
        return lastVTime - firstVTime;
    else
        return 0;

    //return m_dataQueue.mediaLength();
}

static const int MAX_DATA_QUEUE_SIZE = 120;

void QnRtspDataConsumer::cleanupQueueToPos(QnDataPacketQueue::RandomAccess& unsafeQueue, int lastIndex, quint32 ch)
{
    int currentIndex = lastIndex;
    if (m_videoChannels == 1)
    {
        for (int i = 0; i < lastIndex; ++i)
            unsafeQueue.popFront();
        currentIndex = 0;
        if (lastIndex > 0)
            m_someDataIsDropped = true;
    }
    else
    {
        for (int i = lastIndex - 1; i >= 0; --i)
        {
            const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(unsafeQueue.at(i).get());
            if (!video || video->channelNumber == ch)
            {
                unsafeQueue.removeAt(i);
                --currentIndex;
                m_someDataIsDropped = true;
            }
        }
    }

    // clone packet. Put to queue new copy because data is modified
    if (m_someDataIsDropped)
    {
        QnAbstractMediaDataPtr media = QnAbstractMediaDataPtr(std::dynamic_pointer_cast<const QnAbstractMediaData>(unsafeQueue.at(currentIndex))->clone());
        media->flags |= QnAbstractMediaData::MediaFlags_AfterDrop;
        unsafeQueue.setAt(media, currentIndex);
    }
}

void QnRtspDataConsumer::putData(const QnAbstractDataPacketPtr& nonConstData)
{
    QnMutexLocker lock( &m_dataQueueMtx );
    m_dataQueue.push(nonConstData);

    // quality control

    if (m_dataQueue.size() > MAX_DATA_QUEUE_SIZE ||
       (m_dataQueue.size() > m_dataQueue.maxSize() && dataQueueDuration() > TO_LOWQ_SWITCH_MIN_QUEUE_DURATION))
    {
        auto unsafeQueue = m_dataQueue.lock();
        bool clearHiQ = !needSecondaryStream(m_liveQuality); // remove LQ packets, keep HQ

        // try to reduce queue by removed packets in specified quality
        bool somethingDeleted = false;
        for (quint32 ch = 0; ch < m_videoChannels; ++ch)
        {
            for (int i = unsafeQueue.size() - 1; i >=0; --i)
            {
                const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(unsafeQueue.at(i).get() );
                if (video && (video->flags & AV_PKT_FLAG_KEY) && video->channelNumber == ch)
                {
                    bool isHiQ = !(video->flags & QnAbstractMediaData::MediaFlags_LowQuality);
                    if (isHiQ == clearHiQ)
                    {
                        cleanupQueueToPos(unsafeQueue, i, ch);
                        break;
                    }
                }
            }
        }

        // try to reduce queue by removed video packets at any quality
        if (!somethingDeleted)
        {
            for (quint32 ch = 0; ch < m_videoChannels; ++ch)
            {
                for (int i = unsafeQueue.size() - 1; i >=0 ; --i)
                {
                    const QnCompressedVideoData* video = dynamic_cast<const QnCompressedVideoData*>(unsafeQueue.at(i).get() );
                    if (video && (video->flags & AV_PKT_FLAG_KEY) && video->channelNumber == ch)
                    {
                        cleanupQueueToPos(unsafeQueue, i, ch);
                        break;
                    }
                }
            }
        }
    }

    // Queue to large. Clear data anyway causing video artifacts
    while(m_dataQueue.size() > MAX_DATA_QUEUE_SIZE * (int) m_videoChannels)
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
    QnMutexLocker lock( &m_qualityChangeMutex );
    if (m_liveQuality != liveQuality)
        m_newLiveQuality = liveQuality;
}

/*
QnMediaContextPtr QnRtspDataConsumer::getGeneratedContext(AVCodecID compressionType)
{
    QMap<AVCodecID, QnMediaContextPtr>::iterator itr = m_generatedContext.find(compressionType);
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
            NX_ASSERT(!codecCtxData.isEmpty());
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
        //QnMutexLocker lock( &m_owner->getSockMutex() );
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

void QnRtspDataConsumer::setStreamingSpeed(int speed)
{
    NX_ASSERT( speed > 0 );
    m_streamingSpeed = speed <= 0 ? 1 : speed;
}

void QnRtspDataConsumer::setMultiChannelVideo(bool value)
{
    m_multiChannelVideo = value;
}

void QnRtspDataConsumer::doRealtimeDelay(QnConstAbstractMediaDataPtr media)
{
    if (m_rtStartTime == (qint64)AV_NOPTS_VALUE) {
        m_rtStartTime = media->timestamp;
    }
    else {
        qint64 timeDiff = media->timestamp - m_lastRtTime;
        if (timeDiff <= MAX_FRAME_DURATION_MS*1000)
            m_adaptiveSleep.terminatedSleep(timeDiff, MAX_FRAME_DURATION_MS*1000); // if diff too large, it is recording hole. do not calc delay for this case
    }
    m_lastRtTime = media->timestamp;
}

void QnRtspDataConsumer::sendMetadata(const QByteArray& metadata)
{
    RtspServerTrackInfo* metadataTrack = m_owner->getTrackInfo(m_owner->getMetadataChannelNum());
    if (metadataTrack && metadataTrack->clientPort != -1)
    {
        int dataStartIndex = m_sendBuffer.size();
        if (m_owner->isTcpMode())
            m_sendBuffer.resize(m_sendBuffer.size() + kRtpTcpHeaderSize);
        char* rtpHeaderPtr = m_sendBuffer.data() + m_sendBuffer.size();
        m_sendBuffer.resize(m_sendBuffer.size() + RtpHeader::RTP_HEADER_SIZE);
        QnRtspEncoder::buildRTPHeader(rtpHeaderPtr, METADATA_SSRC, metadata.size(), qnSyncTime->currentMSecsSinceEpoch(), RTP_METADATA_CODE, metadataTrack->sequence);
        m_sendBuffer.write(metadata);

        if (m_owner->isTcpMode())
        {
            m_sendBuffer.data()[dataStartIndex] = '$';
            m_sendBuffer.data()[dataStartIndex + 1] = metadataTrack->clientPort;
            quint16* lenPtr = (quint16*) (m_sendBuffer.data() + dataStartIndex + 2);
            *lenPtr = htons(m_sendBuffer.size() - kRtpTcpHeaderSize - dataStartIndex);

            if (m_sendBuffer.size() >= kTcpSendBlockSize)
            {
                m_owner->sendBuffer(m_sendBuffer);
                m_sendBuffer.clear();
            }
        }
        else  if (metadataTrack->mediaSocket)
        {
            NX_ASSERT(m_sendBuffer.size() < 16384);
            metadataTrack->mediaSocket->send(m_sendBuffer.data(), m_sendBuffer.size());
            m_sendBuffer.clear();
        }
        metadataTrack->sequence++;
    }
}

QByteArray QnRtspDataConsumer::getRangeHeaderIfChanged()
{
    QSharedPointer<QnArchiveStreamReader> archiveDP = m_owner->getArchiveDP();
    if (!archiveDP)
        return QByteArray();
    qint64 endTime = archiveDP->endTime();
    if (QnRecordingManager::instance()->isCameraRecoring(archiveDP->getResource()))
        endTime = DATETIME_NOW;

    if (archiveDP->startTime() != m_prevStartTime || endTime != m_prevEndTime) {
        m_prevStartTime = archiveDP->startTime();
        m_prevEndTime = endTime;
        return m_owner->getRangeStr();
    }
    else {
        return QByteArray();
    }
};

void QnRtspDataConsumer::setNeedKeyData()
{
    m_needKeyData.fill(true);
}

bool QnRtspDataConsumer::processData(const QnAbstractDataPacketPtr& nonConstData)
{
    QnConstAbstractDataPacketPtr data = nonConstData;

    if (m_pauseNetwork)
        return false; // does not ready to process data. please wait

    //msleep(500);

    QnConstAbstractMediaDataPtr media = std::dynamic_pointer_cast<const QnAbstractMediaData>(data);
    if (!media || media->channelNumber > CL_MAX_CHANNELS)
        return true;


    const auto flushBuffer = makeScopeGuard(
        [this]()
        {
            if (m_dataQueue.isEmpty() && m_sendBuffer.size() > 0)
            {
                m_owner->sendBuffer(m_sendBuffer);
                m_sendBuffer.clear();
            }
        });

    if( (m_streamingSpeed != MAX_STREAMING_SPEED) && (m_streamingSpeed != 1) )
    {
        NX_ASSERT( !media->flags.testFlag(QnAbstractMediaData::MediaFlags_LIVE) );
        //TODO #ak changing packet's timestamp. It is OK for archive, but generally unsafe.
            //Introduce safe solution
        if( !media->flags.testFlag(QnAbstractMediaData::MediaFlags_LIVE) )
            (static_cast<QnAbstractMediaData*>(nonConstData.get()))->timestamp /= m_streamingSpeed;
    }

    bool isLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;
    bool isVideo = media->dataType == QnAbstractMediaData::VIDEO;
    bool isAudio = media->dataType == QnAbstractMediaData::AUDIO;
    if (isVideo || isAudio)
    {
        bool isKeyFrame = media->flags & AV_PKT_FLAG_KEY;
        bool isSecondaryProvider = media->flags & QnAbstractMediaData::MediaFlags_LowQuality;
        {
            QnMutexLocker lock( &m_qualityChangeMutex );
            if (isKeyFrame && isVideo && m_newLiveQuality != MEDIA_Quality_None)
            {
                if (needSecondaryStream(m_newLiveQuality) && isSecondaryProvider)
                {
                    setLiveQualityInternal(m_newLiveQuality); // slow network. Reduce quality
                    m_newLiveQuality = MEDIA_Quality_None;
                    setNeedKeyData();
                }
                else if (!needSecondaryStream(m_newLiveQuality) && !isSecondaryProvider)
                {
                    setLiveQualityInternal(m_newLiveQuality);
                    m_newLiveQuality = MEDIA_Quality_None;
                    setNeedKeyData();
                }
            }
        }
        if (isLive)
        {
            if (!needSecondaryStream(m_liveQuality) && isSecondaryProvider)
                return true; // data for other live quality stream
            else if (needSecondaryStream(m_liveQuality) && !isSecondaryProvider)
                return true; // data for other live quality stream

            if (isVideo)
            {
                if (isKeyFrame)
                {
                    m_needKeyData[media->channelNumber] = false;
                }
                else if (m_needKeyData[media->channelNumber] ||
                    m_liveQuality == MEDIA_Quality_LowIframesOnly)
                {
                    return true; // wait for I frame for this channel
                }
            }
        }
    }

    int trackNum = media->channelNumber;
    if (!m_multiChannelVideo && media->dataType == QnAbstractMediaData::VIDEO)
        trackNum = 0; // multichannel video is going to be transcoded to a single track
    RtspServerTrackInfo* trackInfo = m_owner->getTrackInfo(trackNum);

    if (trackInfo == nullptr || trackInfo->clientPort == -1)
        return true; // skip data (for example audio is disabled)
    QnRtspEncoderPtr codecEncoder = trackInfo->getEncoder();
    if (!codecEncoder)
        return true; // skip data (for example audio is disabled)
    {
        QnMutexLocker lock( &m_mutex );
        int cseq = media->opaque;
        if (m_waitSCeq != -1) {
            if (cseq != m_waitSCeq)
                return true; // ignore data
            else if (m_pauseNetwork)
                return false; // wait

            m_waitSCeq = -1;
            m_rtStartTime = AV_NOPTS_VALUE;
            m_adaptiveSleep.afterdelay(); // same as reset call
            m_adaptiveSleep.addQuant(MAX_CLIENT_BUFFER_SIZE_MS); // allow to stream data faster at start streaming for some client prebuffer. value in ms

            m_owner->resetTrackTiming();
        }
        if (m_lastSendTime != DATETIME_NOW)
            m_lastSendTime = media->timestamp;
        m_lastMediaTime = media->timestamp;
    }

    if (m_someDataIsDropped) {
        sendMetadata("drop-report");
        m_someDataIsDropped = false;
    }

    if( (m_streamingSpeed != MAX_STREAMING_SPEED) && (!isLive) )
        doRealtimeDelay(media);

    if (isLive && media->dataType == QnAbstractMediaData::VIDEO)
    {
        QnMutexLocker lock( &m_liveTimingControlMtx );
        if (m_firstLiveTime == (qint64)AV_NOPTS_VALUE) {
            m_liveTimer.restart();
            m_lastLiveTime = m_firstLiveTime = media->timestamp;
        }
    }

    QnRtspFfmpegEncoder* ffmpegEncoder = dynamic_cast<QnRtspFfmpegEncoder*>(codecEncoder.data());
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
    AVRational time_base = {1, (int)codecEncoder->getFrequency() };

    m_owner->notifyMediaRangeUsed(media->timestamp);

    while(!m_needStop)
    {
        int dataStartIndex = m_sendBuffer.size();
        if (m_owner->isTcpMode())
            m_sendBuffer.resize(m_sendBuffer.size() + kRtpTcpHeaderSize); // reserve space for RTP TCP header
        char* rtpHeaderPtr = m_sendBuffer.data() + m_sendBuffer.size();
        if (!codecEncoder->getNextPacket(m_sendBuffer))
        {
            m_sendBuffer.resize(dataStartIndex);
            break;
        }

        while (m_pauseNetwork && !m_needStop)
            QnSleep::msleep(1);
        if (m_waitSCeq != -1)
        {
            m_sendBuffer.clear(); //< New seek is received.
            break;
        }

        bool isRtcp = false;
        if (codecEncoder->isRtpHeaderExists())
        {
            RtpHeader* packet = (RtpHeader*) (rtpHeaderPtr);
            isRtcp = packet->payloadType >= 72 && packet->payloadType <= 76;
            if (isRtcp && m_owner->getTracksCount() == 1)
            {
                // skip RTCP packets is no audio. some clients have problem with it. I don't know why.
                m_sendBuffer.resize(dataStartIndex);
                continue;
            }
        }
        else {
            const qint64 packetTime = av_rescale_q(media->timestamp, r, time_base);
            QnRtspEncoder::buildRTPHeader(rtpHeaderPtr, codecEncoder->getSSRC(), codecEncoder->getRtpMarker(), packetTime, codecEncoder->getPayloadtype(), trackInfo->sequence++);
        }

        if (m_owner->isTcpMode())
        {
            m_sendBuffer.data()[dataStartIndex] = '$';
            m_sendBuffer.data()[dataStartIndex + 1] = isRtcp ? trackInfo->clientRtcpPort : trackInfo->clientPort;
            quint16* lenPtr = (quint16*) (m_sendBuffer.data() + dataStartIndex + 2);
            *lenPtr = htons(m_sendBuffer.size() - kRtpTcpHeaderSize - dataStartIndex);
            if (m_sendBuffer.size() >= kTcpSendBlockSize)
            {
                m_owner->sendBuffer(m_sendBuffer);
                m_sendBuffer.clear();
            }
        }
        else
        {
            NX_ASSERT(m_sendBuffer.size() < 16384);
            AbstractDatagramSocket* mediaSocket = isRtcp ? trackInfo->rtcpSocket : trackInfo->mediaSocket;
            mediaSocket->send(m_sendBuffer.data(), m_sendBuffer.size());
            m_sendBuffer.clear();
        }
    }

    static const int FRAMES_BETWEEN_PLAY_RANGE_CHECK = 20;
    if( (++m_framesSinceRangeCheck) > FRAMES_BETWEEN_PLAY_RANGE_CHECK )
    {
        m_framesSinceRangeCheck = 0;
        const QByteArray& newRange = getRangeHeaderIfChanged();
        if (!newRange.isEmpty())
            sendMetadata(newRange);
    }

    if (m_packetSended++ == MAX_PACKETS_AT_SINGLE_SHOT)
        m_singleShotMode = false;

    QnMutexLocker lock( &m_liveTimingControlMtx );
    if (media->dataType == QnAbstractMediaData::VIDEO && m_lastLiveTime != (qint64)AV_NOPTS_VALUE)
        m_lastLiveTime = media->timestamp;

    return true;
}

QnMutex* QnRtspDataConsumer::dataQueueMutex()
{
    return &m_dataQueueMtx;
}

void QnRtspDataConsumer::addData(const QnAbstractMediaDataPtr& data)
{
    m_dataQueue.push(data);
}

int QnRtspDataConsumer::copyLastGopFromCamera(
    QnVideoCameraPtr camera,
    bool usePrimaryStream,
    qint64 skipTime,
    quint32 cseq,
    bool iFramesOnly)
{
    // Fast channel zapping
    int prevSize = m_dataQueue.size();
    int copySize = 0;
    if (camera) // && !res->hasFlags(Qn::no_last_gop))
        copySize = camera->copyLastGop(usePrimaryStream, skipTime, m_dataQueue, cseq, iFramesOnly);
    m_dataQueue.setMaxSize(m_dataQueue.size()-prevSize + MAX_QUEUE_SIZE);
    m_fastChannelZappingSize = copySize;

    QnMutexLocker lock( &m_liveTimingControlMtx );
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
    auto unsafeQueue = m_dataQueue.lock();
    if (unsafeQueue.size() == 0)
        return m_lastMediaTime;
    else
    {
        const QnAbstractMediaData* media = dynamic_cast<const QnAbstractMediaData*>(unsafeQueue.last().get() );
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
    QnMutexLocker lock( &m_dataQueueMtx );
    QnAbstractDataConsumer::clearUnprocessedData();
    m_newLiveQuality = MEDIA_Quality_None;
    m_dataQueue.setMaxSize(MAX_QUEUE_SIZE);
}

void QnRtspDataConsumer::setUseUTCTime(bool value)
{
    m_useUTCTime = value;
}

void QnRtspDataConsumer::setAllowAdaptiveStreaming(bool value)
{
    m_allowAdaptiveStreaming = value;
}

void QnRtspDataConsumer::setLiveQualityInternal(MediaQuality quality)
{
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    QHostAddress clientAddress = m_owner->getPeerAddress();
    m_liveQuality = quality;
}
