// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "av1_rtp_parser.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::rtp {

using namespace nx::media::av1;

namespace {

static constexpr int kAv1ClockRate = 90'000;

static constexpr uint8_t kContinuesPreviousObuMask = 0x80;
static constexpr uint8_t kContinuesInNextPacketMask = 0x40;
static constexpr uint8_t kObuElementCountMask = 0x30;

} // namespace

void Av1Parser::AggregationHeader::decode(uint8_t value)
{
    continuesPreviousObu = value & kContinuesPreviousObuMask;
    continuesInNextPacket = value & kContinuesInNextPacketMask;
    obuElementCount = (value & kObuElementCountMask) >> 4;
    // Bit 3 is N, which is advisory, and the 3 least significant bits are reserved and must be
    // ignored rather than rejected.
}

Av1Parser::Av1Parser()
{
    StreamParser::setFrequency(kAv1ClockRate);
}

Result Av1Parser::processData(const RtpHeader& rtpHeader,
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    bool& gotData)
{
    gotData = false;

    NX_ASSERT(rtpBufferBase, "RTP buffer can not be null.");
    if (!rtpBufferBase)
    {
        clear();
        return {false, "RTP buffer can not be null."};
    }

    const uint32_t rtpTimestamp = rtpHeader.getTimestamp();
    const uint16_t sequenceNumber = rtpHeader.getSequence();

    // Some cameras send duplicate packets. The payload of this one is already in the temporal
    // unit, and appending it again would duplicate its OBUs.
    if (m_lastSequenceNumber == sequenceNumber)
    {
        NX_VERBOSE(this, "%1: duplicate RTP packet, sequence number %2", logId(), sequenceNumber);
        return {true};
    }

    // A lost packet corrupts the OBU stream, so the whole temporal unit is dropped. RtpParser
    // performs the same sequence number check, but it only reports the result upwards through its
    // packetLoss out-param, which RtspStreamProvider feeds to the error counter and never passes
    // down, so it has to be re-derived here. Logged at VERBOSE rather than DEBUG so as not to
    // duplicate RtpParser's line: what this one adds is the effect on the temporal unit.
    if (m_lastSequenceNumber && uint16_t(*m_lastSequenceNumber + 1) != sequenceNumber)
    {
        NX_VERBOSE(this,
            "%1: dropping the temporal unit, RTP packet loss: sequence number %2 followed by %3",
            logId(),
            *m_lastSequenceNumber,
            sequenceNumber);
        dropTemporalUnit();
    }
    m_lastSequenceNumber = sequenceNumber;

    // The previous temporal unit is over: its last packet had the marker bit, or the RTP
    // timestamp has just changed because the sender does not set the marker bit.
    const bool timestampChanged = m_lastRtpTimestamp && *m_lastRtpTimestamp != rtpTimestamp;
    if (m_temporalUnitComplete || timestampChanged)
    {
        if (!m_temporalUnitComplete && !m_temporalUnit.empty())
            NX_VERBOSE(this, "%1: temporal unit ended without the marker bit", logId());
        finishTemporalUnit(m_lastRtpTimestamp.value_or(rtpTimestamp), &gotData);
    }
    m_lastRtpTimestamp = rtpTimestamp;

    Result result = handlePayload(rtpBufferBase + bufferOffset, bytesRead);
    if (!result.success)
    {
        dropTemporalUnit();
        return result;
    }

    if (rtpHeader.marker)
    {
        if (m_hasPendingObuFragment)
        {
            dropTemporalUnit();
            return {false, "Truncated OBU fragment at the end of a temporal unit"};
        }
        m_temporalUnitComplete = true;
        // Never produce two frames from a single call: m_mediaData holds one frame only. The
        // temporal unit stays complete and is emitted on the next call.
        if (!gotData)
            finishTemporalUnit(rtpTimestamp, &gotData);
    }

    if (!gotData && (int) (m_temporalUnit.size() + m_pendingObu.size()) > MAX_ALLOWED_FRAME_SIZE)
    {
        clear();
        NX_WARNING(this, "%1: RTP parser buffer overflow.", logId());
        return {false, "RTP buffer overflow."};
    }

    return {true};
}

Result Av1Parser::handlePayload(const uint8_t* payload, int payloadLength)
{
    if (payloadLength < 1)
        return {false, "Empty AV1 RTP payload"};

    AggregationHeader aggregationHeader;
    aggregationHeader.decode(payload[0]);
    ++payload;
    --payloadLength;

    if (payloadLength == 0)
        return {false, "AV1 RTP payload does not contain OBU elements"};

    if (aggregationHeader.continuesPreviousObu != m_hasPendingObuFragment)
    {
        // Either a continuation without a start, or a fragment whose continuation never arrived.
        // Both are normal consequences of packet loss, so this is not reported as an error.
        NX_VERBOSE(this,
            "%1: broken OBU fragment sequence, Z=%2, has pending fragment=%3",
            logId(),
            aggregationHeader.continuesPreviousObu,
            m_hasPendingObuFragment);
        dropTemporalUnit();
    }

    int elementIndex = 0;
    while (payloadLength > 0)
    {
        ++elementIndex;
        int elementSize = 0;
        const bool isLastDeclaredElement = aggregationHeader.obuElementCount != 0
            && elementIndex == aggregationHeader.obuElementCount;
        if (isLastDeclaredElement)
        {
            // The last element has no length field if the element count is known.
            elementSize = payloadLength;
        }
        else
        {
            uint64_t length = 0;
            const int lengthFieldSize = readLeb128(payload, payloadLength, &length);
            if (lengthFieldSize == 0)
                return {false, "Failed to read the OBU element length"};

            payload += lengthFieldSize;
            payloadLength -= lengthFieldSize;
            if ((uint64_t) payloadLength < length)
            {
                return {false,
                    NX_FMT("OBU element length %1 exceeds the rest of the payload %2",
                        length,
                        payloadLength)};
            }
            elementSize = (int) length;
        }

        if (elementSize == 0)
            return {false, "Zero sized OBU element"};

        const bool isContinuation = aggregationHeader.continuesPreviousObu && elementIndex == 1;
        const bool willContinue =
            aggregationHeader.continuesInNextPacket && payloadLength == elementSize;

        Result result = handleObuElement(payload, elementSize, isContinuation, willContinue);
        if (!result.success)
            return result;

        payload += elementSize;
        payloadLength -= elementSize;
    }

    if (aggregationHeader.obuElementCount != 0
        && elementIndex != aggregationHeader.obuElementCount)
    {
        return {false,
            NX_FMT("The aggregation header declares %1 OBU elements, got %2",
                aggregationHeader.obuElementCount,
                elementIndex)};
    }

    return {true};
}

Result Av1Parser::handleObuElement(
    const uint8_t* data, int size, bool isContinuation, bool willContinue)
{
    if (!isContinuation && !willContinue)
        return appendObu(data, size); //< The whole OBU is in this element.

    if (isContinuation)
        m_pendingObu.insert(m_pendingObu.end(), data, data + size);
    else
        m_pendingObu.assign(data, data + size);

    m_hasPendingObuFragment = willContinue;
    if (willContinue)
        return {true};

    Result result = appendObu(m_pendingObu.data(), (int) m_pendingObu.size());
    m_pendingObu.clear();
    return result;
}

Result Av1Parser::appendObu(const uint8_t* data, int size)
{
    // The temporal unit is not going to be emitted, and the data may be the tail of an OBU whose
    // beginning has been lost, so it can not be parsed.
    if (m_temporalUnitDropped)
        return {true};

    ObuHeader obuHeader;
    const int obuHeaderSize = obuHeader.decode(data, size);
    if (obuHeaderSize == 0)
        return {false, "Failed to decode the OBU header"};

    const uint8_t* payload = data + obuHeaderSize;
    int payloadSize = size - obuHeaderSize;

    if (obuHeader.hasSizeField)
    {
        // The packetizer is expected to strip obu_size, since it duplicates the element length,
        // but not all of them do.
        uint64_t obuSize = 0;
        const int sizeFieldSize = readLeb128(payload, payloadSize, &obuSize);
        if (sizeFieldSize == 0)
            return {false, "Failed to read obu_size"};

        payload += sizeFieldSize;
        payloadSize -= sizeFieldSize;
        if ((uint64_t) payloadSize < obuSize)
            return {false, "obu_size exceeds the OBU element"};

        if ((uint64_t) payloadSize != obuSize)
        {
            NX_VERBOSE(this,
                "%1: %2 extra byte(s) after the OBU payload",
                logId(),
                payloadSize - (int) obuSize);
            payloadSize = (int) obuSize;
        }
    }

    switch (obuHeader.type)
    {
        case ObuType::temporalDelimiter: //< A new one is generated per temporal unit.
        case ObuType::tileList: //< Must not be transmitted, ffmpeg muxers drop it as well.
        case ObuType::padding:
            return {true};
        case ObuType::sequenceHeader:
        case ObuType::frameHeader:
        case ObuType::tileGroup:
        case ObuType::metadata:
        case ObuType::frame:
        case ObuType::redundantFrameHeader:
            break;
        default:
            NX_VERBOSE(
                this, "%1: dropping OBU of a reserved type %2", logId(), (int) obuHeader.type);
            return {true};
    }

    updateContext(obuHeader, payload, payloadSize);

    if (m_waitingForSequenceHeader)
    {
        // Start emitting at a sequence header: a decoder can not use anything before it.
        if (obuHeader.type != ObuType::sequenceHeader)
            return {true};
        m_waitingForSequenceHeader = false;
    }

    if (obuHeader.type == ObuType::frame || obuHeader.type == ObuType::frameHeader)
        m_frameInTemporalUnit = true;

    if (m_temporalUnit.empty())
    {
        m_temporalUnit.insert(m_temporalUnit.end(),
            kTemporalDelimiterObu,
            kTemporalDelimiterObu + sizeof(kTemporalDelimiterObu));
    }

    uint8_t prefix[ObuHeader::kMaxSize + kMaxLeb128Bytes];
    int prefixSize = obuHeader.encode(prefix, /*withSizeField*/ true);
    prefixSize += writeLeb128(payloadSize, prefix + prefixSize);

    m_temporalUnit.insert(m_temporalUnit.end(), prefix, prefix + prefixSize);
    m_temporalUnit.insert(m_temporalUnit.end(), payload, payload + payloadSize);
    return {true};
}

void Av1Parser::updateContext(const ObuHeader& obuHeader, const uint8_t* payload, int size)
{
    switch (obuHeader.type)
    {
        case ObuType::sequenceHeader:
        {
            m_sequenceHeaderInTemporalUnit = true;

            SequenceHeader sequenceHeader;
            if (!sequenceHeader.read(payload, size))
            {
                NX_DEBUG(this, "%1: failed to parse the AV1 sequence header", logId());
                return;
            }
            m_context.sequenceHeader = sequenceHeader;
            m_context.width = sequenceHeader.maxFrameWidth;
            m_context.height = sequenceHeader.maxFrameHeight;
            break;
        }
        case ObuType::frame:
        case ObuType::frameHeader:
        {
            if (!m_context.sequenceHeader)
                return;

            // uncompressed_header() starts the payload, see the AV1 spec, 5.9.2. In a reduced
            // still picture header stream the frame is a key frame by definition.
            if (m_context.sequenceHeader->reducedStillPictureHeader)
            {
                m_keyFrameInTemporalUnit = true;
            }
            else if (size > 0)
            {
                const bool showExistingFrame = payload[0] & 0x80;
                const int frameType = (payload[0] >> 5) & 0x03;
                constexpr int kKeyFrame = 0;
                if (!showExistingFrame && frameType == kKeyFrame)
                    m_keyFrameInTemporalUnit = true;
            }
            break;
        }
        default:
            break;
    }
}

bool Av1Parser::isKeyTemporalUnit() const
{
    if (!m_sequenceHeaderInTemporalUnit)
        return false; //< A decoder can not start here.

    // Cameras may repeat the sequence header in every temporal unit, so it is not enough by
    // itself. If it can not be parsed, trust the sequence header alone.
    return !m_context.sequenceHeader || m_keyFrameInTemporalUnit;
}

void Av1Parser::finishTemporalUnit(uint32_t rtpTimestamp, bool* gotData)
{
    if (!m_temporalUnitDropped && !m_temporalUnit.empty())
    {
        // A temporal unit without a frame header is not decodable. This is how the loss of the
        // first packets of a unit is caught: the loss itself is detected, but it can not be told
        // whether the lost packets belonged to the unit that is ending or to the one that is
        // starting, so without this check the OBUs that did arrive would be emitted as a frame.
        if (m_frameInTemporalUnit)
        {
            m_mediaData = createVideoData(rtpTimestamp);
            *gotData = true;
        }
        else
        {
            NX_VERBOSE(this, "%1: dropping a temporal unit without a frame header", logId());
        }
    }

    m_temporalUnit.clear();
    m_pendingObu.clear();
    m_hasPendingObuFragment = false;
    m_temporalUnitDropped = false;
    m_temporalUnitComplete = false;
    m_sequenceHeaderInTemporalUnit = false;
    m_keyFrameInTemporalUnit = false;
    m_frameInTemporalUnit = false;
}

void Av1Parser::dropTemporalUnit()
{
    m_temporalUnitDropped = true;
    m_temporalUnit.clear();
    m_pendingObu.clear();
    m_hasPendingObuFragment = false;
    m_sequenceHeaderInTemporalUnit = false;
    m_keyFrameInTemporalUnit = false;
    m_frameInTemporalUnit = false;
}

QnCompressedVideoDataPtr Av1Parser::createVideoData(uint32_t rtpTime)
{
    auto result = std::make_shared<QnWritableCompressedVideoData>(m_temporalUnit.size());
    result->m_data.uncheckedWrite((const char*) m_temporalUnit.data(), m_temporalUnit.size());
    result->compressionType = AV_CODEC_ID_AV1;
    result->width = m_context.width;
    result->height = m_context.height;
    result->timestamp = rtpTime;

    if (isKeyTemporalUnit())
    {
        result->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        updateCodecParameters();
    }
    result->context = m_codecParameters;
    return result;
}

void Av1Parser::updateCodecParameters()
{
    auto codecParameters = std::make_shared<CodecParameters>();
    auto avCodecParameters = codecParameters->getAvCodecParameters();
    avCodecParameters->codec_type = AVMEDIA_TYPE_VIDEO;
    avCodecParameters->codec_id = AV_CODEC_ID_AV1;
    avCodecParameters->width = m_context.width;
    avCodecParameters->height = m_context.height;
    if (m_context.sequenceHeader)
    {
        avCodecParameters->profile = m_context.sequenceHeader->seqProfile;
        avCodecParameters->level = m_context.sequenceHeader->seqLevelIdx0;
    }

    if (!m_codecParameters || !m_codecParameters->isEqual(*codecParameters))
        m_codecParameters = codecParameters;
}

void Av1Parser::setSdpInfo(const Sdp::Media& sdp)
{
    if (sdp.rtpmap.clockRate > 0)
    {
        if (sdp.rtpmap.clockRate != kAv1ClockRate)
        {
            NX_WARNING(this,
                "%1: unexpected AV1 clock rate %2, expected %3",
                logId(),
                sdp.rtpmap.clockRate,
                kAv1ClockRate);
        }
        StreamParser::setFrequency(sdp.rtpmap.clockRate);
    }
    m_context.rtpChannel = sdp.payloadType;
}

void Av1Parser::clear()
{
    dropTemporalUnit();
    m_temporalUnitDropped = false;
    m_temporalUnitComplete = false;
    m_lastRtpTimestamp.reset();
    m_lastSequenceNumber.reset();
    // Unlike H.264/H.265 there are no out of band parameter sets in the SDP to fall back to, so
    // the stream can only be resumed at the next in band sequence header.
    m_waitingForSequenceHeader = true;
}

} // namespace nx::rtp
