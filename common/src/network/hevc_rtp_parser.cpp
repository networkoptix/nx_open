#include "hevc_rtp_parser.h"

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>

#include <utils/media/hevc_sps.h>
#include <utils/common/synctime.h>

namespace nx {
namespace network {
namespace rtp {

namespace {

static const int kUninitializedPacketSequenceNumber = -1;
static const int kInvalidHeaderSize = -1;
static const nx::Buffer kSdpRtpMapPrefix("a=rtpmap:");
static const nx::Buffer kSdpFmtpPrefix("a=fmtp:");
static const nx::Buffer kSdpCodecNamePrefix("H265");

static const nx::Buffer kSdpVpsPrefix("sprop-vps");
static const nx::Buffer kSdpSpsPrefix("sprop-sps");
static const nx::Buffer kSdpPpsPrefix("sprop-pps");
static const nx::Buffer kSdpSeiPrefix("sprop-sei");

} // namespace

namespace hevc = nx::media_utils::hevc;

bool HevcParser::processData(
    quint8* rtpBufferBase,
    int rtpBufferOffset,
    int bytesRead,
    const QnRtspStatistic& statistics,
    bool& gotData)
{
    gotData = false;

    NX_ASSERT(rtpBufferBase, "RTP buffer can not be null.");
    if (!rtpBufferBase)
        return reset();

    m_rtpBufferBase = rtpBufferBase;

    bool skipData = false;
    uint32_t rtpTimestamp = 0;
    int payloadLength = bytesRead;
    auto payload = rtpBufferBase + rtpBufferOffset;

    if (!processRtpHeader(&payload, &payloadLength, &skipData, &rtpTimestamp))
    {
        if (!skipData)
            reset();

        return skipData;
    }

    if (!handlePayload(payload, payloadLength))
        return reset();

    createVideoDataIfNeeded(&gotData, statistics);

    m_lastRtpTime = rtpTimestamp;
    return true;
}

void HevcParser::setSDPInfo(QByteArrayList lines)
{
    for (const auto& line: lines)
    {
        auto trimmed = line.trimmed();
        if (trimmed.startsWith(kSdpRtpMapPrefix))
            parseRtpMap(trimmed);
    }

    for (const auto& line: lines)
    {
        auto trimmed = line.trimmed();
        if (trimmed.startsWith(kSdpFmtpPrefix))
            parseFmtp(trimmed);
    }
}

void HevcParser::parseRtpMap(const nx::Buffer& rtpMapLine)
{
    auto values = rtpMapLine.split(' ');
    if (values.size() < 2)
        return;

    auto codecName = values[1];
    if (!codecName.toUpper().startsWith(kSdpCodecNamePrefix))
        return;

    auto values2 = codecName.split('/');
    if (values2.size() < 2)
        return;;

    bool success = false;
    auto frequency = values2[1].toInt(&success);
    if (!success)
        return;

    m_context.frequency = frequency;

    values = values[0].split(':');
    if (values.size() < 2)
        return;

    auto rtpChannel = values[1].toInt(&success);
    if(!success)
        return;

    m_context.rtpChannel = rtpChannel;
}

void HevcParser::parseFmtp(const nx::Buffer& fmtpLine)
{
    int valueIndex = fmtpLine.indexOf(' ');
    if (valueIndex == -1)
        return;

    auto fmtParam = fmtpLine.left(valueIndex).split(':');
    if (fmtParam.size() < 2 || fmtParam[1].toInt() != m_context.rtpChannel)
        return;

    auto hevcParams = fmtpLine.mid(valueIndex + 1).split(';');
    for (const auto& param : hevcParams)
    {
        auto trimmedParam = param.trimmed();
        bool isParameterSet = trimmedParam.startsWith(kSdpVpsPrefix)
            || trimmedParam.startsWith(kSdpSpsPrefix)
            || trimmedParam.startsWith(kSdpPpsPrefix)
            || trimmedParam.startsWith(kSdpSeiPrefix);

        if (!isParameterSet)
            continue;

        int pos = param.indexOf('=');
        if (pos == -1)
            continue;

        auto parameterSet = nx::Buffer::fromBase64(param.mid(pos + 1));
        nx::Buffer startCode(
            (char*)hevc::kNalUnitPrefix,
            sizeof(hevc::kNalUnitPrefix));
        nx::Buffer startCodeShort(
            (char*)hevc::kShortNalUnitPrefix,
            sizeof(hevc::kShortNalUnitPrefix));

        if (parameterSet.endsWith(startCode))
        {
            parameterSet.remove(
                parameterSet.size() - sizeof(hevc::kNalUnitPrefix),
                sizeof(hevc::kNalUnitPrefix));
        }
        else if (parameterSet.endsWith(startCodeShort))
        {
            parameterSet.remove(
                parameterSet.size() - sizeof(hevc::kShortNalUnitPrefix),
                sizeof(hevc::kShortNalUnitPrefix));
        }

        if (parameterSet.startsWith(startCode))
            parameterSet.remove(0, sizeof(hevc::kNalUnitPrefix));
        else if (parameterSet.startsWith(startCodeShort))
            parameterSet.remove(0, sizeof(hevc::kShortNalUnitPrefix));

        if (trimmedParam.startsWith(kSdpVpsPrefix))
        {
            m_context.spropVps = parameterSet;
        }
        else if (trimmedParam.startsWith(kSdpSpsPrefix))
        {
            m_context.spropSps = parameterSet;
            extractPictureDimensionsFromSps(parameterSet);
        }
        else if (trimmedParam.startsWith(kSdpPpsPrefix))
        {
            m_context.spropPps = parameterSet;
        }
        else if (trimmedParam.startsWith(kSdpSeiPrefix))
        {
            m_context.spropSei = parameterSet;
        }
    }
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

bool HevcParser::processRtpHeader(
    uint8_t** payload,
    int* payloadLength,
    bool* outSkipData,
    uint32_t* outRtpTimestamp)
{
    NX_ASSERT(payload && *payload, "Incorrect payload.");
    if (!payload || !*payload)
        return false;

    NX_ASSERT(payloadLength, "Incorrect payload length.");
    if (!payloadLength)
        return false;

    if (outSkipData)
        *outSkipData = false;

    if (*payloadLength < RtpHeader::RTP_HEADER_SIZE + 1)
        return false;

    auto rtpHeaderSize = calculateFullRtpHeaderSize(*payload, *payloadLength);
    if (rtpHeaderSize < RtpHeader::RTP_HEADER_SIZE)
        return false;

    auto rtpHeader = (RtpHeader*)(*payload);
    if (!isApropriatePayloadType(rtpHeader))
    {
        if (outSkipData)
            *outSkipData = true;
        return false;
    }

    if (detectPacketLoss(rtpHeader))
    {
        return handlePacketLoss(
            m_previousPacketSequenceNumber,
            ntohs(rtpHeader->sequence));
    }

    if (rtpHeader->padding)
        *payloadLength -= *((*payload) + (*payloadLength) - 1);

    *payload += rtpHeaderSize;
    *payloadLength -= rtpHeaderSize;

    if (*payloadLength < 1)
        return false;

    if (rtpHeader->marker)
        m_hasEnoughRawData = true; //< It is time to create some video data.

    *outRtpTimestamp = ntohl(rtpHeader->timestamp);

    return true;
}

int HevcParser::calculateFullRtpHeaderSize(
    const uint8_t* rtpHeaderStart,
    int bufferSize) const
{
    //Buffer size should be at least RtpHeader::RTP_HEADER_SIZE bytes long.
    auto rtpHeader = (RtpHeader*) rtpHeaderStart;
    int headerSize = RtpHeader::RTP_HEADER_SIZE +
        rtpHeader->CSRCCount * RtpHeader::CSRC_SIZE;

    if (headerSize > bufferSize)
        return kInvalidHeaderSize;

    if (rtpHeader->extension)
    {
        if(headerSize + RtpHeader::EXTENSION_HEADER_SIZE < bufferSize)
            return kInvalidHeaderSize;

        auto extension = (RtpHeaderExtension*)rtpHeader + headerSize;
        headerSize += RtpHeader::EXTENSION_HEADER_SIZE;

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
    NX_LOGX("Packet loss detected.", cl_logWARNING);
    qDebug() << "Packet loss" << previousSequenceNumber << currentSequenceNumber; //< TODO: #dmishin remove
    emit packetLostDetected(previousSequenceNumber, currentSequenceNumber);
    return reset();
}

bool HevcParser::isApropriatePayloadType(const RtpHeader* rtpHeader) const
{
    // TODO: #dmishin implement
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
        case hevc::PacketType::SingleNalUnitPacket:
            return handleSingleNalUnitPacket(&packetHeader, payload, payloadLength);
        case hevc::PacketType::FragmentationPacket:
            return handleFragmentationPacket(&packetHeader, payload, payloadLength);
        case hevc::PacketType::AggregationPacket:
            return handleAggregationPacket(&packetHeader, payload, payloadLength);
        case hevc::PacketType::PaciPacket:
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
    if (header->unitType == hevc::NalUnitType::SpsNut)
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
    const hevc::NalUnitHeader* header,
    const uint8_t* payload,
    int payloadLength)
{
    DonType donType = DonType::Donl;
    while (payloadLength)
    {
        skipDonIfNeeded(&payload, &payloadLength, donType);
        donType = DonType::Dond;

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

        if (payloadLength < 0)
            return false;
    }

    return true;
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
    }

    m_chunks.emplace_back(payload - m_rtpBufferBase, payloadLength, fuHeader.startFlag);
    m_videoFrameSize += payloadLength;

    if (fuHeader.endFlag)
        ++m_numberOfNalUnits;

    return true;
}

bool HevcParser::handlePaciPacket(
    const nx::media_utils::hevc::NalUnitHeader* header,
    const uint8_t* payload,
    int payloadLength)
{
    NX_LOGX("HEVC RTP packet handling is not implemented yet.", cl_logWARNING);
    return false;
}

bool HevcParser::skipDonIfNeeded(
    const uint8_t** outPayload,
    int* outPayloadLength,
    DonType donType)
{
    if (m_context.spropMaxDonDiff)
    {
        auto bytesToSkip = donType == DonType::Donl ? 2 : 1;
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
    if (unitType == hevc::NalUnitType::VpsNut)
    {
        m_context.inStreamVpsFound = true;
    }
    else if (unitType == hevc::NalUnitType::SpsNut)
    {
        m_context.inStreamSpsFound = true;
        extractPictureDimensionsFromSps(payload, payloadLength);
    }
    else if (unitType == hevc::NalUnitType::PpsNut)
    {
        m_context.inStreamPpsFound = true;
    }
    else if(hevc::isRandomAccessPoint(unitType))
    {
        m_context.keyDataFound = true;
    }
}

QnCompressedVideoDataPtr HevcParser::createVideoData(
    const uint8_t* rtpBuffer,
    uint32_t rtpTime,
    const QnRtspStatistic& statistics)
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
                (const char*)hevc::kNalUnitPrefix,
                sizeof(hevc::kNalUnitPrefix));
        }

        result->m_data.uncheckedWrite(
            (const char*)rtpBuffer + m_chunks[i].bufferOffset,
            m_chunks[i].len);
    }

    if (m_timeHelper)
        result->timestamp = m_timeHelper->getUsecTime(rtpTime, statistics, m_context.frequency);
    else
        result->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;
    
    return result;
}

void HevcParser::createVideoDataIfNeeded(bool* outGotData, const QnRtspStatistic& statistic)
{
    if (m_hasEnoughRawData)
    {
        m_mediaData = createVideoData(m_rtpBufferBase, m_lastRtpTime, statistic);
        *outGotData = !!m_mediaData;
        reset(/*softReset*/true);
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

    m_hasEnoughRawData = false;
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
        additionalBufferSize += sizeof(hevc::kNalUnitPrefix) + m_context.spropVps->size();
    if (!m_context.inStreamSpsFound && m_context.spropSps)
        additionalBufferSize += sizeof(hevc::kNalUnitPrefix) + m_context.spropSps->size();
    if (!m_context.inStreamPpsFound && m_context.spropPps)
        additionalBufferSize += sizeof(hevc::kNalUnitPrefix) + m_context.spropPps->size();

    // Space for NAL prefixes
    additionalBufferSize += m_numberOfNalUnits * sizeof(hevc::kNalUnitPrefix);

    return additionalBufferSize;
}

void HevcParser::addSdpParameterSetsIfNeeded(QnByteArray& buffer)
{
    if (!m_context.inStreamVpsFound && m_context.spropVps)
    {
        buffer.uncheckedWrite(
            (char*)hevc::kNalUnitPrefix,
            sizeof(hevc::kNalUnitPrefix));

        buffer.uncheckedWrite(
            m_context.spropVps.get().data(),
            m_context.spropVps.get().size());
    }
    if (!m_context.inStreamSpsFound && m_context.spropSps)
    {
        buffer.uncheckedWrite(
            (char*)hevc::kNalUnitPrefix,
            sizeof(hevc::kNalUnitPrefix));
        buffer.uncheckedWrite(
            m_context.spropSps.get().data(),
            m_context.spropSps.get().size());
    }
    if (!m_context.inStreamPpsFound && m_context.spropPps)
    {
        buffer.uncheckedWrite(
            (char*)hevc::kNalUnitPrefix,
            sizeof(hevc::kNalUnitPrefix));
        buffer.uncheckedWrite(
            m_context.spropPps.get().data(),
            m_context.spropPps.get().size());
    }
}

} // namespace rtp
} // namespace network
} // namespace nx
