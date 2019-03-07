#include "hevc_rtp_parser.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

#include <utils/media/hevc_sps.h>
#include <utils/common/synctime.h>
#include <utils/media/nalUnits.h>

namespace nx::streaming::rtp {

namespace {

static const int kUninitializedPacketSequenceNumber = -1;
static const int kInvalidHeaderSize = -1;

static const nx::Buffer kSdpVpsPrefix("sprop-vps");
static const nx::Buffer kSdpSpsPrefix("sprop-sps");
static const nx::Buffer kSdpPpsPrefix("sprop-pps");
static const nx::Buffer kSdpSeiPrefix("sprop-sei");

} // namespace

namespace hevc = nx::media_utils::hevc;

HevcParser::HevcParser()
{
    StreamParser::setFrequency(90000);
}

bool HevcParser::processData(
    quint8* rtpBufferBase,
    int rtpBufferOffset,
    int bytesRead,
    bool& gotData)
{
    gotData = false;

    NX_ASSERT(rtpBufferBase, "RTP buffer can not be null.");
    if (!rtpBufferBase)
        return reset();

    m_rtpBufferBase = rtpBufferBase;

    bool isFatalError = false;
    uint32_t rtpTimestamp = 0;
    uint16_t sequenceNumber = 0;
    int payloadLength = bytesRead;
    auto payload = rtpBufferBase + rtpBufferOffset;

    if (!processRtpHeader(&payload, &payloadLength, &isFatalError, &rtpTimestamp, &sequenceNumber))
    {
        if (isFatalError)
            reset();

        return !isFatalError;
    }

    if (rtpTimestamp == m_lastCreatedPacketTimestamp)
    {
        // Skip data if it belongs to the previously created frame.
        // Don't trust RTP marker bit anymore, since the stream is considered buggy in this case.
        m_trustMarkerBit = false;
        return true;
    }

    if (rtpTimestamp != m_lastRtpTimestamp && m_lastRtpTimestamp != m_lastCreatedPacketTimestamp)
    {
        // We got new frame, but there wasn't marker bit in the end of previous.
        m_trustMarkerBit = false;
    }

    if (!m_trustMarkerBit)
    {
        createVideoDataIfNeeded(&gotData, rtpTimestamp);
        // Restore pointer to the buffer after data creation. It will be needed for payload processing.
        m_rtpBufferBase = rtpBufferBase;
    }

    if (!handlePayload(payload, payloadLength))
        return reset();

    if (!m_trustMarkerBit && rtpTimestamp != m_lastRtpTimestamp)
        backupCurrentData(rtpBufferBase);

    if (m_trustMarkerBit)
        createVideoDataIfNeeded(&gotData, rtpTimestamp);

    m_lastRtpTimestamp = rtpTimestamp;

    // Check buffer overflow
    if (!gotData && m_videoFrameSize > (int) MAX_ALLOWED_FRAME_SIZE)
    {
        NX_WARNING(this, "RTP parser buffer overflow.");
        handlePacketLoss(m_previousPacketSequenceNumber, sequenceNumber);
        return reset();
    }
    return true;
}

void HevcParser::setSdpInfo(const Sdp::Media& sdp)
{
    if (sdp.rtpmap.clockRate > 0)
        StreamParser::setFrequency(sdp.rtpmap.clockRate);
    m_context.rtpChannel = sdp.payloadType;
    parseFmtp(sdp.fmtp.params);
}

void HevcParser::parseFmtp(const QStringList& fmtpParams)
{
    for (const auto& param : fmtpParams)
    {
        bool isParameterSet = param.startsWith(kSdpVpsPrefix)
            || param.startsWith(kSdpSpsPrefix)
            || param.startsWith(kSdpPpsPrefix)
            || param.startsWith(kSdpSeiPrefix);

        if (!isParameterSet)
            continue;

        int pos = param.indexOf('=');
        if (pos == -1)
            continue;

        auto parameterSet = nx::Buffer::fromBase64(param.mid(pos + 1).toUtf8());
        // Some cameras (e.g. DigitalWatchdog)
        // may send extra start code in parameter set SDP string.
        parameterSet = NALUnit::dropBorderedStartCodes(parameterSet);
        if (param.startsWith(kSdpVpsPrefix))
        {
            m_context.spropVps = parameterSet;
        }
        else if (param.startsWith(kSdpSpsPrefix))
        {
            m_context.spropSps = parameterSet;
            extractPictureDimensionsFromSps(parameterSet);
        }
        else if (param.startsWith(kSdpPpsPrefix))
        {
            m_context.spropPps = parameterSet;
        }
        else if (param.startsWith(kSdpSeiPrefix))
        {
            m_context.spropSei = parameterSet;
        }
    }
}

//-------------------------------------------------------------------------------------------------

bool HevcParser::processRtpHeader(
    uint8_t** outPayload,
    int* outPayloadLength,
    bool* outIsFatalError,
    uint32_t* outRtpTimestamp,
    uint16_t* sequenceNumber)
{
    NX_ASSERT(outPayload && *outPayload, "Incorrect payload.");
    if (!outPayload || !*outPayload)
        return false;

    NX_ASSERT(outPayloadLength, "Incorrect payload length.");
    if (!outPayloadLength)
        return false;

    auto& payload = *outPayload;
    auto& payloadLength = *outPayloadLength;

    if (outIsFatalError)
        *outIsFatalError = true;

    if (payloadLength < RtpHeader::kSize + 1)
        return false;

    auto rtpHeaderSize = calculateFullRtpHeaderSize(payload, payloadLength);
    if (rtpHeaderSize < RtpHeader::kSize)
        return false;

    auto rtpHeader = (RtpHeader*)(payload);
    if (!isApropriatePayloadType(rtpHeader))
    {
        if (outIsFatalError)
            *outIsFatalError = false;
        return false;
    }

    *sequenceNumber = ntohs(rtpHeader->sequence);
    if (detectPacketLoss(rtpHeader))
    {
        return handlePacketLoss(
            m_previousPacketSequenceNumber,
            *sequenceNumber);
    }

    if (rtpHeader->padding)
        payloadLength -= *(payload + payloadLength - 1);

    payload += rtpHeaderSize;
    payloadLength -= rtpHeaderSize;

    if (payloadLength < 1)
        return false;

    if (rtpHeader->marker)
        m_gotMarkerBit = true;

    *outRtpTimestamp = ntohl(rtpHeader->timestamp);

    return true;
}

int HevcParser::calculateFullRtpHeaderSize(
    const uint8_t* rtpHeaderStart,
    int bufferSize) const
{
    //Buffer size should be at least RtpHeader::kSize bytes long.
    auto rtpHeader = (RtpHeader*) rtpHeaderStart;
    int headerSize = rtpHeader->payloadOffset();

    if (headerSize > bufferSize)
        return kInvalidHeaderSize;

    if (rtpHeader->extension)
    {
        if (bufferSize < headerSize + RtpHeaderExtension::kSize)
            return kInvalidHeaderSize;

        auto extension = (RtpHeaderExtension*)(rtpHeaderStart + headerSize);
        headerSize += RtpHeaderExtension::kSize;

        const int kWordSize = 4;
        headerSize += ntohs(extension->length) * kWordSize;
        if (bufferSize < headerSize)
            return kInvalidHeaderSize;
    }

    return headerSize;
}

bool HevcParser::detectPacketLoss(const RtpHeader* rtpHeader)
{
    if (m_previousPacketSequenceNumber == kUninitializedPacketSequenceNumber)
    {
        m_previousPacketSequenceNumber = ntohs(rtpHeader->sequence);
        return false;
    }

    bool packetLoss = false;
    auto currentPacketSequenceNumber = ntohs(rtpHeader->sequence);
    if (currentPacketSequenceNumber - m_previousPacketSequenceNumber > 1)
        packetLoss = true;

    m_previousPacketSequenceNumber = currentPacketSequenceNumber;

    return packetLoss;
}

bool HevcParser::handlePacketLoss(
    int previousSequenceNumber,
    int currentSequenceNumber)
{
    NX_DEBUG(this, "Packet loss detected");
    emit packetLostDetected(previousSequenceNumber, currentSequenceNumber);
    return reset();
}

bool HevcParser::isApropriatePayloadType(const RtpHeader* rtpHeader) const
{
    return rtpHeader->payloadType == m_context.rtpChannel;
}

bool HevcParser::handlePayload(const uint8_t* payload, int payloadLength)
{
    if (payloadLength <= 0)
        return false;

    hevc::NalUnitHeader packetHeader;
    if (!packetHeader.decode(payload, payloadLength))
        return false;

    updateNalFlags(packetHeader.unitType, payload, payloadLength);
    skipPayloadHeader(&payload, &payloadLength);

    auto packetType = hevc::fromNalUnitTypeToPacketType(packetHeader.unitType);
    switch (packetType)
    {
        case hevc::PacketType::singleNalUnitPacket:
            return handleSingleNalUnitPacket(&packetHeader, payload, payloadLength);
        case hevc::PacketType::fragmentationPacket:
            return handleFragmentationPacket(&packetHeader, payload, payloadLength);
        case hevc::PacketType::aggregationPacket:
            return handleAggregationPacket(&packetHeader, payload, payloadLength);
        case hevc::PacketType::paciPacket:
            return handlePaciPacket(&packetHeader, payload, payloadLength);
        default:
            return false;
    }
}

bool HevcParser::handleSingleNalUnitPacket(
    const hevc::NalUnitHeader* header,
    const uint8_t* payload,
    int payloadLength)
{
    bool skipped = skipDonIfNeeded(&payload, &payloadLength);

    if (payloadLength < 0)
        return false;

    if (skipped)
    {
        insertPayloadHeader(
            const_cast<uint8_t**>(&payload), //< Yep, it's a hack
            &payloadLength,
            header->unitType,
            header->tid);
    }
    else
    {
        goBackForPayloadHeader(&payload, &payloadLength);
    }

    updateNalFlags(header->unitType, payload, payloadLength);
    if (header->unitType == hevc::NalUnitType::spsNut)
    {
        hevc::Sps sps;
        sps.decode(payload, payloadLength);
        m_context.width = sps.picWidthInLumaSamples;
        m_context.height = sps.picHeightInLumaSamples;
    }

    m_chunks.emplace_back(payload - m_rtpBufferBase, payloadLength, true);
    m_videoFrameSize += payloadLength;
    ++m_numberOfNalUnits;
    return true;
}

bool HevcParser::handleAggregationPacket(
    const hevc::NalUnitHeader* /*header*/,
    const uint8_t* payload,
    int payloadLength)
{
    DonType donType = DonType::donl;
    while (payloadLength > 0)
    {
        skipDonIfNeeded(&payload, &payloadLength, donType);
        donType = DonType::dond;

        auto nalSize = ntohs(*(uint16_t*)payload);
        payload += 2; //< Skip size field.
        payloadLength -= 2;

        if (payloadLength < nalSize || nalSize < hevc::NalUnitHeader::kTotalLength)
            return false;

        hevc::NalUnitHeader nalHeader;
        if (!nalHeader.decode(payload, payloadLength))
            return false;

        updateNalFlags(nalHeader.unitType, payload, payloadLength);

        m_chunks.emplace_back(payload - m_rtpBufferBase, nalSize, true);
        m_videoFrameSize += nalSize;
        ++m_numberOfNalUnits;

        payload += nalSize;
        payloadLength -= nalSize;
    }

    return payloadLength == 0;
}

bool HevcParser::handleFragmentationPacket(
    const hevc::NalUnitHeader* header,
    const uint8_t* payload,
    int payloadLength)
{
    hevc::FuHeader fuHeader;
    if(!fuHeader.decode(payload, payloadLength))
        return false;

    if (fuHeader.startFlag && fuHeader.endFlag)
        return false;

    skipFuHeader(&payload, &payloadLength);
    skipDonIfNeeded(&payload, &payloadLength);

    if (payloadLength < 0)
        return false;

    if (fuHeader.startFlag)
    {
        insertPayloadHeader(
            const_cast<uint8_t**>(&payload),  //< Dirty dirty hack.
            &payloadLength,
            fuHeader.unitType,
            header->tid);
        updateNalFlags(fuHeader.unitType, payload, payloadLength);
        ++m_numberOfNalUnits;
    }

    m_chunks.emplace_back(payload - m_rtpBufferBase, payloadLength, fuHeader.startFlag);
    m_videoFrameSize += payloadLength;

    return true;
}

bool HevcParser::handlePaciPacket(
    const nx::media_utils::hevc::NalUnitHeader* /*header*/,
    const uint8_t* /*payload*/,
    int /*payloadLength*/)
{
    NX_WARNING(this, "HEVC PACI RTP packet handling is not implemented yet.");
    return false;
}

bool HevcParser::skipDonIfNeeded(
    const uint8_t** outPayload,
    int* outPayloadLength,
    DonType donType)
{
    if (m_context.spropMaxDonDiff)
    {
        auto bytesToSkip = donType == DonType::donl ? 2 : 1;
        *outPayload += bytesToSkip;
        *outPayloadLength -= bytesToSkip;
    }

    return m_context.spropMaxDonDiff;
}

void HevcParser::skipPayloadHeader(const uint8_t** outPayload, int* outPayloadLength)
{
    *outPayload += hevc::NalUnitHeader::kTotalLength;
    *outPayloadLength -= hevc::NalUnitHeader::kTotalLength;
}

void HevcParser::skipFuHeader(const uint8_t** outPayload, int* outPayloadLength)
{
    *outPayload += hevc::FuHeader::kTotalLength;
    *outPayloadLength -= hevc::FuHeader::kTotalLength;
}

void HevcParser::goBackForPayloadHeader(const uint8_t** outPayload, int* outPayloadLength)
{
    *outPayload -= hevc::NalUnitHeader::kTotalLength;
    *outPayloadLength += hevc::NalUnitHeader::kTotalLength;
}

void HevcParser::updateNalFlags(
    hevc::NalUnitType unitType,
    const uint8_t* payload,
    int payloadLength)
{
    if (unitType == hevc::NalUnitType::vpsNut)
    {
        m_context.inStreamVpsFound = true;
    }
    else if (unitType == hevc::NalUnitType::spsNut)
    {
        m_context.inStreamSpsFound = true;
        extractPictureDimensionsFromSps(payload, payloadLength);
    }
    else if (unitType == hevc::NalUnitType::ppsNut)
    {
        m_context.inStreamPpsFound = true;
    }
    else if(hevc::isRandomAccessPoint(unitType))
    {
        m_context.keyDataFound = true;
    }
}

QnCompressedVideoDataPtr HevcParser::createVideoData(const uint8_t* rtpBuffer, uint32_t rtpTime)
{
    int totalSize = m_videoFrameSize + additionalBufferSize();

    QnWritableCompressedVideoDataPtr result =
        QnWritableCompressedVideoDataPtr(
            new QnWritableCompressedVideoData(
                CL_MEDIA_ALIGNMENT,
                totalSize));

    result->compressionType = AV_CODEC_ID_H265;
    result->width = m_context.width;
    result->height = m_context.height;

    if (m_context.keyDataFound)
    {
        result->flags = QnAbstractMediaData::MediaFlags_AVKey;
        addSdpParameterSetsIfNeeded(result->m_data);
    }

    for (size_t i = 0; i < m_chunks.size(); ++i)
    {
        if (m_chunks[i].nalStart)
        {
            result->m_data.uncheckedWrite(
                (const char*)NALUnit::kStartCodeLong,
                sizeof(NALUnit::kStartCodeLong));
        }

        const auto chunkBufferStart = m_chunks[i].bufferStart
            ? (const char*) m_chunks[i].bufferStart
            : (const char*) rtpBuffer;

        result->m_data.uncheckedWrite(
            chunkBufferStart + m_chunks[i].bufferOffset,
            m_chunks[i].len);
    }

    result->timestamp = rtpTime;
    return result;
}

void HevcParser::createVideoDataIfNeeded(bool* outGotData, uint32_t rtpTimestamp)
{
    bool needToCreateVideoData = (m_trustMarkerBit && m_gotMarkerBit)
        || (!m_trustMarkerBit && !m_chunks.empty() && rtpTimestamp != m_lastRtpTimestamp);

    if (needToCreateVideoData)
    {
        m_mediaData = createVideoData(m_rtpBufferBase, m_lastRtpTimestamp);
        m_lastCreatedPacketTimestamp = m_lastRtpTimestamp;
        *outGotData = !!m_mediaData;
        reset(/*softReset*/ true);
    }
}

bool HevcParser::reset(bool softReset)
{
    m_videoFrameSize = 0;
    if (m_context.keyDataFound || !softReset)
    {
        m_context.inStreamVpsFound = false;
        m_context.inStreamSpsFound = false;
        m_context.inStreamPpsFound = false;
    }

    m_context.keyDataFound = false;

    m_gotMarkerBit = false;
    m_numberOfNalUnits = 0;
    m_rtpBufferBase = nullptr;
    m_chunks.clear();

    return false;
}

bool HevcParser::extractPictureDimensionsFromSps(const nx::Buffer& buffer)
{
    return extractPictureDimensionsFromSps((const uint8_t*)buffer.data(), buffer.size());
}

bool HevcParser::extractPictureDimensionsFromSps(const uint8_t* buffer, int bufferLength)
{
    hevc::Sps sps;
    if (!sps.decode(buffer, bufferLength))
        return false;

    m_context.width = sps.picWidthInLumaSamples;
    m_context.height = sps.picHeightInLumaSamples;

    return true;
}

void HevcParser::insertPayloadHeader(
    uint8_t** payloadStart,
    int* payloadLength,
    nx::media_utils::hevc::NalUnitType unitType,
    uint8_t tid)
{
    *payloadStart -= hevc::NalUnitHeader::kTotalLength;
    *payloadLength += hevc::NalUnitHeader::kTotalLength;
    (*payloadStart)[0] = ((uint8_t)unitType) << 1;
    (*payloadStart)[1] = tid;
}

int HevcParser::additionalBufferSize() const
{
    int additionalBufferSize = 0;

    // Space for parameter sets with NAL prefixes
    if (!m_context.inStreamVpsFound && m_context.spropVps)
        additionalBufferSize += sizeof(NALUnit::kStartCodeLong) + m_context.spropVps->size();
    if (!m_context.inStreamSpsFound && m_context.spropSps)
        additionalBufferSize += sizeof(NALUnit::kStartCodeLong) + m_context.spropSps->size();
    if (!m_context.inStreamPpsFound && m_context.spropPps)
        additionalBufferSize += sizeof(NALUnit::kStartCodeLong) + m_context.spropPps->size();

    // Space for NAL prefixes
    additionalBufferSize += m_numberOfNalUnits * sizeof(NALUnit::kStartCodeLong);

    return additionalBufferSize;
}

void HevcParser::addSdpParameterSetsIfNeeded(QnByteArray& buffer)
{
    if (!m_context.inStreamVpsFound && m_context.spropVps)
    {
        buffer.uncheckedWrite(
            (char*)NALUnit::kStartCodeLong,
            sizeof(NALUnit::kStartCodeLong));

        buffer.uncheckedWrite(
            m_context.spropVps.get().data(),
            m_context.spropVps.get().size());
    }
    if (!m_context.inStreamSpsFound && m_context.spropSps)
    {
        buffer.uncheckedWrite(
            (char*)NALUnit::kStartCodeLong,
            sizeof(NALUnit::kStartCodeLong));
        buffer.uncheckedWrite(
            m_context.spropSps.get().data(),
            m_context.spropSps.get().size());
    }
    if (!m_context.inStreamPpsFound && m_context.spropPps)
    {
        buffer.uncheckedWrite(
            (char*)NALUnit::kStartCodeLong,
            sizeof(NALUnit::kStartCodeLong));
        buffer.uncheckedWrite(
            m_context.spropPps.get().data(),
            m_context.spropPps.get().size());
    }
}

} // namespace nx::streaming::rtp
