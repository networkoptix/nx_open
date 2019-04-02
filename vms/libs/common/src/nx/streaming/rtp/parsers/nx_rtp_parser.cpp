#include "nx_rtp_parser.h"

#include <nx/utils/log/log.h>

#include <analytics/common/object_detection_metadata.h>

#include <nx/streaming/rtp/rtp.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/basic_media_context.h>
#include <nx/streaming/config.h>
#include <nx/streaming/nx_streaming_ini.h>

#include <nx/debugging/visual_metadata_debugger_factory.h>

#include <motion/motion_detection.h>
#include <nx/utils/log/log_main.h>

using namespace nx::debugging;

namespace nx::streaming::rtp {

static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;
static const int RTSP_FFMPEG_VIDEO_HEADER_SIZE = 3;
static const int RTSP_FFMPEG_METADATA_HEADER_SIZE = 8; //< m_duration + metadataType

QnNxRtpParser::QnNxRtpParser(const QString& debugSourceId):
    VideoStreamParser(),
    m_debugSourceId(debugSourceId),
    m_nextDataPacketBuffer(nullptr),
    m_position(AV_NOPTS_VALUE),
    m_isAudioEnabled(true),
    m_visualDebugger(VisualMetadataDebuggerFactory::makeDebugger(DebuggerType::nxRtpParser))
{
    if (nxStreamingIni().analyticsMetadataLogFilePrefix[0])
    {
        m_analyticsMetadataLogFile.setFileName(
            QLatin1String(nxStreamingIni().analyticsMetadataLogFilePrefix)
                + m_debugSourceId + lit(".log"));
        if (!m_analyticsMetadataLogFile.open(QIODevice::WriteOnly))
        {
            NX_ERROR(this) << lm("Unable to create analytics metadata log file \"%1\".")
                .arg(m_analyticsMetadataLogFile.fileName());
        }
        else
        {
            NX_WARNING(this) << lm("Created analytics metadata log file \"%1\".")
                .arg(m_analyticsMetadataLogFile.fileName());
            m_isAnalyticsMetadataLogFileOpened = true;
        }
    }
}

QnNxRtpParser::~QnNxRtpParser()
{
    if (m_isAnalyticsMetadataLogFileOpened)
        m_analyticsMetadataLogFile.close();
}

void QnNxRtpParser::writeAnalyticsMetadataToLogFile(const QnAbstractMediaDataPtr& metadata)
{
    nx::common::metadata::DetectionMetadataPacketPtr data =
        nx::common::metadata::fromMetadataPacket(
            std::dynamic_pointer_cast<QnCompressedMetadata>(metadata));
    if (!data)
    {
        NX_ERROR(this) << "Unable to deserialize analytics metadata.";
        return;
    }

    const QString metadataStr = toString(*data);
    const qint64 dPtsUs = m_nextDataPacket->timestamp - m_lastFramePtsUs;
    m_analyticsMetadataLogFile.write(
        lm("dPts %1%2 ms\n").args(dPtsUs >= 0 ? "+" : "", (500 + dPtsUs) / 1000).toUtf8());
    m_analyticsMetadataLogFile.write(metadataStr.toUtf8());
    m_analyticsMetadataLogFile.flush();
}

bool QnNxRtpParser::processData(quint8* rtpBufferBase, int bufferOffset, int dataSize, bool& gotData)
{
    gotData = false;
    if (dataSize < RtpHeader::kSize)
    {
        NX_WARNING(this,
            lm("Invalid RTP packet size=%1. size < RTP header size. Ignored.").arg(dataSize));
        return false;
    }
    const quint8* data = rtpBufferBase + bufferOffset;
    RtpHeader* rtpHeader = (RtpHeader*) data;

    if (rtpHeader->CSRCCount != 0 || rtpHeader->version != 2)
    {
        NX_WARNING(this, lm("Got mailformed RTP packet header. Ignored."));
        return false;
    }

    const quint8* payload = data + RtpHeader::kSize;
    dataSize -= RtpHeader::kSize;
    // Odd numbers - codec context, even numbers - data.
    const bool isCodecContext = qFromBigEndian(rtpHeader->ssrc) & 1;
    if (isCodecContext)
    {
        m_context = QnConstMediaContextPtr(QnBasicMediaContext::deserialize(
            QByteArray((const char*) payload, dataSize)));
    }
    else
    {
        QnConstMediaContextPtr context = m_context;

        if (rtpHeader->padding)
            dataSize -= qFromBigEndian(rtpHeader->padding);
        if (dataSize < 1)
            return false;

        if (!m_nextDataPacket)
        {
            if (dataSize < RTSP_FFMPEG_GENERIC_HEADER_SIZE)
                return false;

            const QnAbstractMediaData::DataType dataType =
                (QnAbstractMediaData::DataType) *(payload++);
            quint32 timestampHigh = qFromBigEndian(*(quint32*) payload);
            dataSize -= RTSP_FFMPEG_GENERIC_HEADER_SIZE;
            payload  += 4; // deserialize timeStamp high part

            quint8 cseq = *payload++;
            quint16 flags = (payload[0]<<8) + payload[1];
            payload += 2;

            if (dataType == QnAbstractMediaData::EMPTY_DATA)
            {
                QnEmptyMediaData* emptyMediaData = new QnEmptyMediaData();
                m_nextDataPacket = QnEmptyMediaDataPtr(emptyMediaData);
                m_nextDataPacketBuffer = &emptyMediaData->m_data;
                if (dataSize != 0)
                {
                    NX_WARNING(
                        this,
                        lm("Unexpected data size for EOF/BOF packet. Got %1 expected %2 bytes.")
                        .arg(dataSize).arg(0));
                    return false;
                }
            }
            else if (dataType == QnAbstractMediaData::META_V1
                || dataType == QnAbstractMediaData::GENERIC_METADATA)
            {
                if (dataType == QnAbstractMediaData::META_V1
                    && dataSize != Qn::kMotionGridWidth*Qn::kMotionGridHeight/8 + RTSP_FFMPEG_METADATA_HEADER_SIZE)
                {
                    NX_WARNING(
                        this,
                        lm("Unexpected data size for metadata packet. Got %1 expected %2 bytes.")
                        .arg(dataSize).arg(Qn::kMotionGridWidth*Qn::kMotionGridHeight / 8));
                    return false;
                }
                if (dataSize < RTSP_FFMPEG_METADATA_HEADER_SIZE)
                    return false;

                auto duration = qFromBigEndian(*((quint32*) payload)) * 1000;
                auto metadataType = (MetadataType) qFromBigEndian(*((quint32*) (payload) + 1));

                QnAbstractCompressedMetadata* metadata = nullptr;

                if (dataType == QnAbstractMediaData::META_V1)
                {
                    metadata = new QnMetaDataV1();
                    metadata->metadataType = MetadataType::Motion;
                }
                else
                {
                    metadata = new QnCompressedMetadata(metadataType);
                    metadata->metadataType = metadataType;
                }

                metadata->m_data.clear();
                context.reset();
                metadata->m_duration = duration;
                dataSize -= RTSP_FFMPEG_METADATA_HEADER_SIZE;
                payload += RTSP_FFMPEG_METADATA_HEADER_SIZE; // deserialize video flags

                m_nextDataPacket = QnAbstractCompressedMetadataPtr(metadata);
                m_nextDataPacketBuffer = &metadata->m_data;
            }
            else if (context && context->getCodecType() == AVMEDIA_TYPE_VIDEO && dataType == QnAbstractMediaData::VIDEO)
            {
                if (dataSize < RTSP_FFMPEG_VIDEO_HEADER_SIZE)
                    return false;

                quint32 fullPayloadLen = (payload[0] << 16) + (payload[1] << 8) + payload[2];

                dataSize -= RTSP_FFMPEG_VIDEO_HEADER_SIZE;
                payload += RTSP_FFMPEG_VIDEO_HEADER_SIZE; // deserialize video flags

                QnWritableCompressedVideoData *video = new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, fullPayloadLen, context);
                m_nextDataPacket = QnCompressedVideoDataPtr(video);
                m_nextDataPacketBuffer = &video->m_data;

                video->width = context->getCodedWidth();
                video->height = context->getCodedHeight();
            }
            else if (context && context->getCodecType() == AVMEDIA_TYPE_AUDIO && dataType == QnAbstractMediaData::AUDIO)
            {
                QnWritableCompressedAudioData *audio = new QnWritableCompressedAudioData(CL_MEDIA_ALIGNMENT, dataSize); // , context
                audio->context = context;
                //audio->format.fromAvStream(context->ctx());
                m_nextDataPacket = QnCompressedAudioDataPtr(audio);
                m_nextDataPacketBuffer = &audio->m_data;
            }
            else
            {
                if (context)
                {
                    NX_WARNING(this, lm("Unsupported codec or packet type %1")
                        .arg(context->getCodecType()));
                }
                else if (dataType == QnAbstractMediaData::AUDIO)
                {
                    NX_WARNING(this, lm("Unsupported audio codec or codec params"));
                }
                else if (dataType == QnAbstractMediaData::VIDEO)
                {
                    NX_WARNING(this, lm("Unsupported video or codec params"));
                }
                else
                {
                    NX_WARNING(this, lm("Unsupported or unknown packet"));
                }
                return false;
            }

            if (m_nextDataPacket)
            {
                m_nextDataPacket->opaque = cseq;
                m_nextDataPacket->flags = static_cast<QnAbstractMediaData::MediaFlags>(flags);

                if (context)
                    m_nextDataPacket->compressionType = context->getCodecId();
                m_nextDataPacket->timestamp =
                    qFromBigEndian(rtpHeader->timestamp) + (qint64(timestampHigh) << 32);

                // TODO: Investigate whether this code is needed.
                //m_nextDataPacket->channelNumber = channelNum;
                /*
                if (mediaPacket->timestamp < 0x40000000 && m_prevTimestamp[ssrc] > 0xc0000000)
                    m_timeStampCycles[ssrc]++;
                mediaPacket->timestamp += ((qint64) m_timeStampCycles[ssrc] << 32);
                */
            }
        }

        if (m_nextDataPacket)
        {
            NX_ASSERT(m_nextDataPacketBuffer);
            m_nextDataPacketBuffer->write((const char*) payload, dataSize);

            if (m_nextDataPacket->dataType == QnAbstractMediaData::VIDEO)
                m_lastFramePtsUs = m_nextDataPacket->timestamp;

            if (nxStreamingIni().analyticsMetadataLogFilePrefix[0]
                && m_nextDataPacket->dataType == QnAbstractMediaData::GENERIC_METADATA)
            {
                writeAnalyticsMetadataToLogFile(m_nextDataPacket);
            }
        }

        if (rtpHeader->marker)
        {
            if (m_nextDataPacket)
            {
                if (m_nextDataPacket->flags & QnAbstractMediaData::MediaFlags_LIVE)
                    m_position = DATETIME_NOW;
                else
                    m_position = m_nextDataPacket->timestamp;
            }
            if (m_nextDataPacket->dataType == QnAbstractMediaData::AUDIO && !m_isAudioEnabled)
            {
                // Skip audio packet.
            }
            else
            {
                m_mediaData = m_nextDataPacket;
                gotData = true;

                if (m_nextDataPacket->dataType == QnAbstractMediaData::DataType::VIDEO)
                {
                    m_visualDebugger->push(
                        std::dynamic_pointer_cast<QnCompressedVideoData>(m_nextDataPacket));
                }

                if (m_nextDataPacket->dataType == QnAbstractMediaData::DataType::GENERIC_METADATA)
                {
                    m_visualDebugger->push(
                        std::dynamic_pointer_cast<QnCompressedMetadata>(m_nextDataPacket));
                }
            }
            m_nextDataPacket.reset(); // EOF video frame reached
            m_nextDataPacketBuffer = nullptr;
        }
    }
    return true;
}

void QnNxRtpParser::setAudioEnabled(bool value)
{
    m_isAudioEnabled = value;
}

} // namespace nx::streaming::rtp
