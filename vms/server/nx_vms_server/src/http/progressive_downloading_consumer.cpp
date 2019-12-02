#include "progressive_downloading_consumer.h"
#include "progressive_downloading_server.h"

#include <common/common_module.h>
#include <transcoding/ffmpeg_transcoder.h>

namespace {
    constexpr int kMaxQueueSize = 30;
}

ProgressiveDownloadingConsumer::ProgressiveDownloadingConsumer(
    ProgressiveDownloadingServer* owner, const Config& config):
    QnAbstractDataConsumer(50),
    m_owner(owner),
    m_config(config)
{
    if (m_config.dropLateFrames)
    {
        m_dataOutput.reset(new CachedOutputStream(owner));
        m_dataOutput->start();
    }
    setObjectName("ProgressiveDownloadingConsumer");
}

ProgressiveDownloadingConsumer::~ProgressiveDownloadingConsumer()
{
    pleaseStop();
    m_adaptiveSleep.breakSleep();
    stop();
    if( m_dataOutput.get() )
        m_dataOutput->stop();
}

void ProgressiveDownloadingConsumer::setAuditHandle(const AuditHandle& handle)
{
    m_auditHandle = handle;
}

void ProgressiveDownloadingConsumer::copyLastGopFromCamera(const QnVideoCameraPtr& camera)
{
    camera->copyLastGop(
        nx::vms::api::StreamIndex::primary,
        /*skipTime*/ 0,
        m_dataQueue,
        /*iFramesOnly*/ false);
    m_dataQueue.setMaxSize(m_dataQueue.size() + kMaxQueueSize);
}

bool ProgressiveDownloadingConsumer::canAcceptData() const
{
    if (m_config.liveMode)
        return true;
    else
        return QnAbstractDataConsumer::canAcceptData();
}

void ProgressiveDownloadingConsumer::putData(const QnAbstractDataPacketPtr& data)
{
    if (m_config.liveMode)
    {
        if (m_dataQueue.size() > m_dataQueue.maxSize())
        {
            m_needKeyData = true;
            return;
        }
    }
    const QnAbstractMediaData* media = dynamic_cast<const QnAbstractMediaData*>(data.get());
    if (media && m_config.audioOnly && media->dataType != QnAbstractMediaData::AUDIO)
        return;

    if (m_needKeyData && media)
    {
        if (!(media->flags & AV_PKT_FLAG_KEY))
            return;
        m_needKeyData = false;
    }
    QnAbstractDataConsumer::putData(data);
}

bool ProgressiveDownloadingConsumer::processData(const QnAbstractDataPacketPtr& data)
{
    if (m_config.standFrameDuration)
        doRealtimeDelay( data );

    const QnAbstractMediaDataPtr& media = std::dynamic_pointer_cast<QnAbstractMediaData>(data);

    if (media->dataType == QnAbstractMediaData::EMPTY_DATA) {
        if (media->timestamp == DATETIME_NOW)
            m_needStop = true; // EOF reached
        finalizeMediaStream();
        return true;
    }

    if (m_config.endTimeUsec != AV_NOPTS_VALUE && media->timestamp > m_config.endTimeUsec)
    {
        m_needStop = true; // EOF reached
        finalizeMediaStream();
        return true;
    }

    if (media && m_auditHandle)
        m_owner->commonModule()->auditManager()->notifyPlaybackInProgress(m_auditHandle, media->timestamp);

    if (media && !(media->flags & QnAbstractMediaData::MediaFlags_LIVE) && m_config.continuousTimestamps)
    {
        if (m_lastMediaTime != (int64_t)AV_NOPTS_VALUE && media->timestamp - m_lastMediaTime > MAX_FRAME_DURATION_MS*1000 &&
            media->timestamp != (int64_t)AV_NOPTS_VALUE && media->timestamp != DATETIME_NOW)
        {
            m_utcShift -= (media->timestamp - m_lastMediaTime) - 1000000/60;
        }
        m_lastMediaTime = media->timestamp;
        media->timestamp += m_utcShift;
    }

    QnByteArray result(CL_MEDIA_ALIGNMENT, 0);
    bool isArchive = !(media->flags & QnAbstractMediaData::MediaFlags_LIVE);

    QnByteArray* const resultPtr =
        (m_dataOutput.get() &&
            !isArchive && // thin out only live frames
            m_dataOutput->packetsInQueue() > m_config.maxFramesToCacheBeforeDrop) ? NULL : &result;

    if (!resultPtr)
    {
        NX_VERBOSE(this, lit("Insufficient bandwidth to %1. Skipping frame...").
            arg(m_owner->getForeignAddress().toString()));
    }
    int errCode = m_owner->getTranscoder()->transcodePacket(
        media,
        resultPtr);   //if previous frame dispatch not even started, skipping current frame
    if (errCode == 0)
    {
        if( resultPtr && result.size() > 0 )
            sendFrame(media->timestamp, result);
    }
    else
    {
        NX_DEBUG(this, lit("Terminating progressive download (url %1) connection from %2 due to transcode error (%3)").
            arg(m_owner->getDecodedUrl().toString()).arg(m_owner->getForeignAddress().toString()).arg(errCode));
        m_needStop = true;
    }

    return true;
}

void ProgressiveDownloadingConsumer::sendFrame(qint64 timestamp, const QnByteArray& result)
{
    //Preparing output packet. Have to do everything right here to avoid additional frame copying
    //TODO shared chunked buffer and socket::writev is wanted very much here
    QByteArray outPacket;
    const auto context = m_owner->getTranscoder()->getVideoCodecContext();
    if (context && context->codec_id == AV_CODEC_ID_MJPEG)
    {
        //preparing timestamp header
        QByteArray timestampHeader;

        // This is for buggy iOS 5, which for some reason stips multipart headers
        if (timestamp != AV_NOPTS_VALUE)
        {
            timestampHeader.append("Content-Type: image/jpeg;ts=");
            timestampHeader.append(QByteArray::number(timestamp, 10));
        }
        else
        {
            timestampHeader.append("Content-Type: image/jpeg");
        }
        timestampHeader.append("\r\n");

        if (timestamp != AV_NOPTS_VALUE)
        {
            timestampHeader.append("x-Content-Timestamp: ");
            timestampHeader.append(QByteArray::number(timestamp, 10));
            timestampHeader.append("\r\n");
        }

        //composing http chunk
        outPacket.reserve(result.size() + 12 + timestampHeader.size());    //12 - http chunk overhead
        outPacket.append(QByteArray::number((int)(result.size() + timestampHeader.size()), 16)); //http chunk
        outPacket.append("\r\n");                           //http chunk
                                                            //skipping delimiter
        const char* delimiterEndPos = (const char*)memchr(result.data(), '\n', result.size());
        if (delimiterEndPos == NULL)
        {
            outPacket.append(result.data(), result.size());
        }
        else
        {
            outPacket.append(result.data(), delimiterEndPos - result.data() + 1);
            outPacket.append(timestampHeader);
            outPacket.append(delimiterEndPos + 1, result.size() - (delimiterEndPos - result.data() + 1));
        }
        outPacket.append("\r\n");       //http chunk
    }
    else
    {
        outPacket.reserve(result.size() + 12);    //12 - http chunk overhead
        outPacket.append(QByteArray::number((int)result.size(), 16)); //http chunk
        outPacket.append("\r\n");                           //http chunk
        outPacket.append(result.data(), result.size());
        outPacket.append("\r\n");       //http chunk
    }

    //sending frame
    if (m_dataOutput.get())
    {   // Wait if bandwidth is not sufficient inside postPacket().
        // This is to ensure that we will send every archive packet.
        // This shouldn't affect live packets, as we thin them out above.
        // Refer to processData() for details.
        m_dataOutput->postPacket(outPacket, m_config.maxFramesToCacheBeforeDrop);
        if (m_dataOutput->failed())
            m_needStop = true;
    }
    else
    {
        if (!m_owner->sendBuffer(outPacket))
            m_needStop = true;
    }
}

QByteArray ProgressiveDownloadingConsumer::toHttpChunk( const char* data, size_t size )
{
    QByteArray chunk;
    chunk.reserve((int) size + 12);
    chunk.append(QByteArray::number((int) size, 16));
    chunk.append("\r\n");
    chunk.append(data, (int) size);
    chunk.append("\r\n");
    return chunk;
}

void ProgressiveDownloadingConsumer::doRealtimeDelay( const QnAbstractDataPacketPtr& media )
{
    if( m_rtStartTime == (int64_t)AV_NOPTS_VALUE )
    {
        m_rtStartTime = media->timestamp;
    }
    else
    {
        int64_t timeDiff = media->timestamp - m_lastRtTime;
        if( timeDiff <= MAX_FRAME_DURATION_MS*1000 )
            m_adaptiveSleep.terminatedSleep(timeDiff, MAX_FRAME_DURATION_MS*1000); // if diff too large, it is recording hole. do not calc delay for this case
    }
    m_lastRtTime = media->timestamp;
}

void ProgressiveDownloadingConsumer::finalizeMediaStream()
{
    QnByteArray result(CL_MEDIA_ALIGNMENT, 0);
    if ((m_owner->getTranscoder()->finalize(&result) == 0) &&
        (result.size() > 0))
    {
        sendFrame(AV_NOPTS_VALUE, result);
    }
}

