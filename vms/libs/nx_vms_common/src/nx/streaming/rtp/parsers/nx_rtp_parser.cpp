// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nx_rtp_parser.h"

#include <analytics/common/object_metadata.h>
#include <nx/analytics/analytics_logging_ini.h>
#include <nx/kit/utils.h>
#include <nx/media/audio_data_packet.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/config.h>
#include <nx/media/motion_detection.h>
#include <nx/media/video_data_packet.h>
#include <nx/rtp/rtp.h>
#include <nx/streaming/nx_streaming_ini.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/math/math.h>
#include <utils/common/synctime.h>

namespace nx::rtp {

static const int RTSP_FFMPEG_GENERIC_HEADER_SIZE = 8;
static const int RTSP_FFMPEG_VIDEO_HEADER_SIZE = 3;
static const int RTSP_FFMPEG_METADATA_HEADER_SIZE = 8; //< m_duration + metadataType

QnNxRtpParser::QnNxRtpParser(nx::Uuid deviceId, const QString& tag):
    base_type()
{
    using namespace nx::analytics;
    m_primaryLogger = std::make_unique<MetadataLogger>(
        nx::format("rtp_parser_%1@%2_").args(tag, nx::kit::utils::toString(this)),
        deviceId,
        /*engineId*/ nx::Uuid(),
        nx::vms::api::StreamIndex::primary);
    m_secondaryLogger = std::make_unique<MetadataLogger>(
        nx::format("rtp_parser_%1@%2_").args(tag, nx::kit::utils::toString(this)),
        deviceId,
        /*engineId*/ nx::Uuid(),
        nx::vms::api::StreamIndex::secondary);
}

QnNxRtpParser::QnNxRtpParser()
{
}

void QnNxRtpParser::setServerVersion(const nx::utils::SoftwareVersion& serverVersion)
{
    m_serverVersion = serverVersion;
}

QnAbstractMediaDataPtr QnNxRtpParser::nextData()
{
    if (!m_mediaData)
        return nullptr;

    QnAbstractMediaDataPtr result;
    result.swap(m_mediaData);
    return result;
}

void QnNxRtpParser::logMediaData(const QnAbstractMediaDataPtr& data)
{
    if (!nx::analytics::loggingIni().isLoggingEnabled() || !data)
        return;

    const bool isSecondaryProvider = data->flags & QnAbstractMediaData::MediaFlags_LowQuality;
    auto& logger = isSecondaryProvider ? m_secondaryLogger : m_primaryLogger;
    if (logger)
        logger->pushData(data);
}

Result QnNxRtpParser::processData(const uint8_t* data, int dataSize, bool& gotData)
{
    gotData = false;
    if (dataSize < RtpHeader::kSize)
    {
        return {false, NX_FMT("Invalid RTP packet size=%1. size < RTP header size. Ignored.", dataSize)};
    }
    const RtpHeader* rtpHeader = (RtpHeader*)data;

    if (rtpHeader->CSRCCount != 0 || rtpHeader->version != 2)
        return {false, "Got malformed RTP packet header. Ignored."};

    const quint8* payload = data + RtpHeader::kSize;
    dataSize -= RtpHeader::kSize;

    return processData(*rtpHeader, payload, dataSize, gotData);
}

Result QnNxRtpParser::processData(
    const RtpHeader& rtpHeader,
    quint8* rtpBufferBase,
    int bufferOffset,
    int dataSize,
    bool& gotData)
{
    return processData(rtpHeader, rtpBufferBase + bufferOffset, dataSize, gotData);
}

Result QnNxRtpParser::processData(
    const RtpHeader& rtpHeader,
    const uint8_t* payload,
    int dataSize,
    bool& gotData)
{
    gotData = false;

    // Odd numbers - codec context, even numbers - data.
    const bool isCodecContext = qFromBigEndian(rtpHeader.ssrc) & 1;
    if (isCodecContext)
    {
        int contextVersion = 0;
        auto context = std::make_shared<CodecParameters>();
        if (qBetween(
            nx::utils::SoftwareVersion(5, 0),
            m_serverVersion,
            nx::utils::SoftwareVersion(5, 0, 34644)))
        {
            contextVersion = 1;
        }

        if (!context->deserialize((const char*) payload, dataSize, contextVersion))
            return {false, "Failed to parse codec parameters from RTP packet."};

        m_context = context;
    }
    else
    {
        CodecParametersConstPtr context = m_context;

        if (rtpHeader.padding)
            dataSize -= qFromBigEndian(rtpHeader.padding);
        if (dataSize < 1)
            return {false, NX_FMT("Unexpected data size %1", dataSize)};

        if (!m_nextDataPacket)
        {
            if (dataSize < RTSP_FFMPEG_GENERIC_HEADER_SIZE)
            {
                return {false, NX_FMT("Unexpected data size %1. "
                    "Expected at least %2 bytes)", dataSize, RTSP_FFMPEG_GENERIC_HEADER_SIZE)};
            }

            QnAbstractMediaData::DataType dataType =
                (QnAbstractMediaData::DataType) *(payload++);
            const quint32 timestampHigh = qFromBigEndian(*(quint32*) payload);
            dataSize -= RTSP_FFMPEG_GENERIC_HEADER_SIZE;
            payload  += 4; // deserialize timeStamp high part

            const quint8 cseq = *payload++;
            const quint16 flags = (payload[0]<<8) + payload[1];
            payload += 2;

            // TODO #lbusygin: just for vms_4.2 version, get rid of this code when the mobile client
            // stops supporting servers < 5.0.
            if (m_context && m_context->version() == 0)
            {
                if (dataType == 3)
                    dataType = QnAbstractMediaData::GENERIC_METADATA;
                if (dataType == 4)
                    dataType = QnAbstractMediaData::EMPTY_DATA;
            }

            if (dataType == QnAbstractMediaData::EMPTY_DATA)
            {
                QnEmptyMediaData* emptyMediaData = new QnEmptyMediaData();
                m_nextDataPacket = QnEmptyMediaDataPtr(emptyMediaData);
                m_nextDataPacketBuffer = &emptyMediaData->m_data;
                if (dataSize != 0)
                {
                    return {false, NX_FMT("Unexpected data size for EOF/BOF packet. "
                        "Got %1 expected %2 bytes.", dataSize, 0)};
                }
            }
            else if (dataType == QnAbstractMediaData::META_V1
                || dataType == QnAbstractMediaData::GENERIC_METADATA)
            {
                if (dataType == QnAbstractMediaData::META_V1
                    && dataSize != Qn::kMotionGridWidth*Qn::kMotionGridHeight/8 + RTSP_FFMPEG_METADATA_HEADER_SIZE)
                {
                    return { false, NX_FMT("Unexpected data size for metadata packet. "
                        "Got %1 expected %2 bytes.",
                        dataSize, Qn::kMotionGridWidth * Qn::kMotionGridHeight / 8)};
                }
                if (dataSize < RTSP_FFMPEG_METADATA_HEADER_SIZE)
                {
                    return { false, NX_FMT("Unexpected data size for metadata packet. "
                        "Got %1 expected %2 bytes.",
                        dataSize, RTSP_FFMPEG_METADATA_HEADER_SIZE)};
                }

                const auto duration = qFromBigEndian(*((quint32*) payload)) * 1000;
                const auto metadataType = (MetadataType) qFromBigEndian(*((quint32*) (payload) + 1));

                QnAbstractCompressedMetadata* metadata = nullptr;

                if (dataType == QnAbstractMediaData::META_V1
                    || metadataType == MetadataType::Motion)
                {
                    metadata = new QnMetaDataV1(qnSyncTime->currentTimePoint());
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
                {
                    return { false, NX_FMT("Unexpected data size for video packet. "
                        "Got %1 expected %2 bytes.",
                        dataSize, RTSP_FFMPEG_VIDEO_HEADER_SIZE)};
                }

                const quint32 fullPayloadLen = (payload[0] << 16) + (payload[1] << 8) + payload[2];

                dataSize -= RTSP_FFMPEG_VIDEO_HEADER_SIZE;
                payload += RTSP_FFMPEG_VIDEO_HEADER_SIZE; // deserialize video flags

                auto video = new QnWritableCompressedVideoData(fullPayloadLen, context);
                m_nextDataPacket = QnCompressedVideoDataPtr(video);
                m_nextDataPacketBuffer = &video->m_data;

                video->width = context->getWidth();
                video->height = context->getHeight();
            }
            else if (context && context->getCodecType() == AVMEDIA_TYPE_AUDIO && dataType == QnAbstractMediaData::AUDIO)
            {
                // TODO: #rvasilenko why don't we pass context in the constructor?
                auto audio = new QnWritableCompressedAudioData(dataSize); // , context
                audio->context = context;
                //audio->format.fromAvStream(context->ctx());
                m_nextDataPacket = QnCompressedAudioDataPtr(audio);
                m_nextDataPacketBuffer = &audio->m_data;
            }
            else
            {
                if (context)
                {
                    NX_WARNING(this, "Unsupported codec or packet type %1, data typa: %2",
                        context->getCodecType(), dataType);
                }
                else if (dataType == QnAbstractMediaData::AUDIO)
                {
                    NX_WARNING(this, nx::format("Unsupported audio codec or codec params"));
                }
                else if (dataType == QnAbstractMediaData::VIDEO)
                {
                    NX_WARNING(this, nx::format("Unsupported video or codec params codec_type: %1"),
                        context ? context->getCodecType() : -2);
                }
                else
                {
                    NX_WARNING(this, nx::format("Unsupported or unknown packet"));
                }
                return {false, NX_FMT("Unexpected packet type %1", dataType)};
            }

            if (!NX_ASSERT(m_nextDataPacket))
                return {false, "Internal parser error. Next packet is expected"};

            m_nextDataPacket->opaque = cseq;
            m_nextDataPacket->flags = static_cast<QnAbstractMediaData::MediaFlags>(flags);

            if (context)
                m_nextDataPacket->compressionType = context->getCodecId();
            m_nextDataPacket->timestamp =
                qFromBigEndian(rtpHeader.timestamp) + (qint64(timestampHigh) << 32);
        }

        NX_VERBOSE(this, "Got data of type %1 for time %2",
            (int)m_nextDataPacket->dataType,
            nx::utils::timestampToDebugString(m_nextDataPacket->timestamp));

        if (NX_ASSERT(m_nextDataPacketBuffer))
            m_nextDataPacketBuffer->write((const char*) payload, dataSize);

        if (m_nextDataPacket->dataType == QnAbstractMediaData::VIDEO)
            m_lastFramePtsUs = m_nextDataPacket->timestamp;

        if (rtpHeader.marker)
        {
            if (m_nextDataPacket->flags & QnAbstractMediaData::MediaFlags_LIVE)
                m_position = DATETIME_NOW;
            else
                m_position = m_nextDataPacket->timestamp;

            if (m_nextDataPacket->dataType == QnAbstractMediaData::AUDIO && !m_isAudioEnabled)
            {
                // Skip audio packet.
            }
            else
            {
                m_mediaData = m_nextDataPacket;
                logMediaData(m_mediaData);
                gotData = true;
            }
            m_nextDataPacket.reset(); // EOF video frame reached
            m_nextDataPacketBuffer = nullptr;
        }
    }
    return { true };
}

void QnNxRtpParser::setAudioEnabled(bool value)
{
    m_isAudioEnabled = value;
}

} // namespace nx::rtp
