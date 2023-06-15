// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hevc_rtp_parser.h"

#include <nx/codec/hevc/sequence_parameter_set.h>
#include <nx/codec/nal_units.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>

namespace nx::rtp {

namespace {

static constexpr char kSdpVpsPrefix[] = "sprop-vps";
static constexpr char kSdpSpsPrefix[] = "sprop-sps";
static constexpr char kSdpPpsPrefix[] = "sprop-pps";
static constexpr char kSdpSeiPrefix[] = "sprop-sei";

bool isKeyFrame(const uint8_t* data, int64_t size)
{
    auto nalUnits = nx::media::nal::findNalUnitsMp4(data, size);
    for (const auto& nalu: nalUnits)
    {
        nx::media::hevc::NalUnitHeader nalHeader;
        if (nalHeader.decode(nalu.data, nalu.size))
        {
            if (nx::media::hevc::isRandomAccessPoint(nalHeader.unitType))
                return true;
        }
    }
    return false;
}

} // namespace

namespace hevc = nx::media::hevc;

HevcParser::HevcParser()
{
    StreamParser::setFrequency(90'000);
}

Result HevcParser::processData(
    const RtpHeader& rtpHeader,
    quint8* rtpBufferBase,
    int rtpBufferOffset,
    int bytesRead,
    bool& gotData)
{
    m_gotData = gotData = false;

    NX_ASSERT(rtpBufferBase, "RTP buffer can not be null.");
    if (!rtpBufferBase)
    {
        reset();
        return {false, "RTP buffer can not be null."};
    }

    m_rtpBufferBase = rtpBufferBase;
    uint32_t rtpTimestamp = rtpHeader.getTimestamp();
    int payloadLength = bytesRead;
    auto payload = rtpBufferBase + rtpBufferOffset;

    Result result = handlePayload(payload, payloadLength);
    if (!result.success)
    {
        reset();
        return result;
    }

    m_lastRtpTimestamp = rtpTimestamp;
    gotData = m_gotData; // Got data can be set in handlePayload
    if (gotData)
    {
        m_chunks.backupCurrentData(rtpBufferBase);
    }
    else if (rtpHeader.marker && m_frameStartFound && m_trustMarkerBit)
    {
        m_mediaData = createVideoData(rtpBufferBase, rtpTimestamp);
        reset(/*softReset*/ true);
        gotData = true;
        m_frameStartFound = false;
    }

    // Check buffer overflow
    if (!gotData && m_chunks.size() > MAX_ALLOWED_FRAME_SIZE)
    {
        reset();
        NX_WARNING(this, "RTP parser buffer overflow.");
        return {false, "RTP buffer overflow."};
    }
    return {true};
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

        auto parameterSet = QByteArray::fromBase64(param.mid(pos + 1).toUtf8());
        // Some cameras (e.g. DigitalWatchdog)
        // may send extra start code in parameter set SDP string.
        parameterSet = NALUnit::dropBorderedStartCodes(parameterSet);
        if (param.startsWith(kSdpVpsPrefix))
        {
            m_context.vps = parameterSet;
        }
        else if (param.startsWith(kSdpSpsPrefix))
        {
            m_context.sps = parameterSet;
            extractPictureDimensionsFromSps((uint8_t*)parameterSet.data(), parameterSet.size());
        }
        else if (param.startsWith(kSdpPpsPrefix))
        {
            m_context.pps = parameterSet;
        }
        else if (param.startsWith(kSdpSeiPrefix))
        {
            m_context.sei = parameterSet;
        }
    }
}

Result HevcParser::handlePayload(const uint8_t* payload, int payloadLength)
{
    if (payloadLength <= 0)
        return {false, "Not enough data in RTP payload"};

    hevc::NalUnitHeader packetHeader;
    QString errorString;
    if (!packetHeader.decode(payload, payloadLength, &errorString))
    {
        return {false, NX_FMT("Can't decode H.265 packet header: %1", errorString)};
    }

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
            return {false, NX_FMT("Unknown packet type %1", (int) packetType)};
    }
}

bool HevcParser::isNewFrame(hevc::NalUnitType unitType, const uint8_t* payload, int payloadLength)
{
    const uint8_t* data = payload;
    const uint8_t* dataEnd = payload + payloadLength;

    // See ffmpeg 4.4 function hevc_find_frame_end.
    bool result = false;
    while (data < dataEnd)
    {
        if (hevc::isNewAccessUnit(unitType))
        {
            if (m_frameStartFound)
            {
                m_frameStartFound = false;
                result = true;
            }
        }
        else if (hevc::isSlice(unitType))
        {
            int first_slice_segment_in_pic_flag = data[2] & 0x80;
            if (first_slice_segment_in_pic_flag)
            {
                // First slice of next frame found
                if (m_frameStartFound)
                    return true;

                m_frameStartFound = true;
            }
            else if (!m_frameStartFound)
            {
                m_trustMarkerBit = false;
            }
            return result;
        }
        data = NALUnit::findNextNAL(data, dataEnd);
        hevc::NalUnitHeader nalHeader;
        if (data < dataEnd && !nalHeader.decode(data, dataEnd - data))
            return false;
        unitType = nalHeader.unitType;
    }
    return result;
}

void HevcParser::addChunk(
    nx::media::hevc::NalUnitType unitType, int bufferOffset, int payloadLength, bool hasStartCode)
{
    const uint8_t* payload = m_rtpBufferBase + bufferOffset;

    if (hasStartCode && isNewFrame(unitType, payload, payloadLength))
    {
        m_mediaData = createVideoData(m_rtpBufferBase, m_lastRtpTimestamp);
        reset(/*softReset*/ true);
        m_gotData = true;
    }

    m_chunks.addChunk(bufferOffset, payloadLength, hasStartCode);
}

Result HevcParser::handleSingleNalUnitPacket(
    const hevc::NalUnitHeader* header,
    const uint8_t* payload,
    int payloadLength)
{
    bool skipped = skipDonIfNeeded(&payload, &payloadLength);

    if (payloadLength < 0)
        return {false, "Not enough data in RTP packet"};

    if (skipped)
    {
        insertPayloadHeader(
            const_cast<uint8_t**>(&payload), //< Yep, it's a bad practice
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
        hevc::SequenceParameterSet sps;
        if (!nx::media::hevc::parseNalUnit(payload, payloadLength, sps))
            return {false, "Failed to parse SPS"};
        m_context.width = sps.width;
        m_context.height = sps.height;
    }

    addChunk(header->unitType, payload - m_rtpBufferBase, payloadLength, true);
    return {true};
}

Result HevcParser::handleAggregationPacket(
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
            return {false, NX_FMT("Invalid payload length %1", payloadLength)};

        hevc::NalUnitHeader nalHeader;
        if (!nalHeader.decode(payload, payloadLength))
            return {false, "Can't decode nal header"};

        updateNalFlags(nalHeader.unitType, payload, payloadLength);

        addChunk(nalHeader.unitType, payload - m_rtpBufferBase, nalSize, true);

        payload += nalSize;
        payloadLength -= nalSize;
    }

    if (payloadLength)
    {
        return {false, NX_FMT(
            "Unexpected payload rest in %1 byte(s) at the end of RTP packet", payloadLength)};
    }

    return {true};
}

Result HevcParser::handleFragmentationPacket(
    const hevc::NalUnitHeader* header,
    const uint8_t* payload,
    int payloadLength)
{
    hevc::FuHeader fuHeader;
    if(!fuHeader.decode(payload, payloadLength))
        return {false, "Can't decode FU header"};

    if (fuHeader.startFlag && fuHeader.endFlag)
        return {false, "Invalid flags in FU header"};

    skipFuHeader(&payload, &payloadLength);
    skipDonIfNeeded(&payload, &payloadLength);

    if (payloadLength < 0)
        return {false, "Not enough data in RTP packet"};

    if (fuHeader.startFlag)
    {
        insertPayloadHeader(
            const_cast<uint8_t**>(&payload),  //< Dirty-dirty practice.
            &payloadLength,
            fuHeader.unitType,
            header->tid);
        updateNalFlags(fuHeader.unitType, payload, payloadLength);
    }

    addChunk(fuHeader.unitType, payload - m_rtpBufferBase, payloadLength, fuHeader.startFlag);

    return {true};
}

Result HevcParser::handlePaciPacket(
    const nx::media::hevc::NalUnitHeader* /*header*/,
    const uint8_t* /*payload*/,
    int /*payloadLength*/)
{
    return {false, "HEVC PACI RTP packet handling is not implemented yet."};
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
    nx::utils::ByteArray header;
    if (m_context.keyDataFound)
        header = buildParameterSetsIfNeeded();

    QnWritableCompressedVideoDataPtr result;
    if (header.size() != 0)
        result = m_chunks.buildFrame(rtpBuffer, (uint8_t*)header.data(), header.size());
    else
        result = m_chunks.buildFrame(rtpBuffer, nullptr, 0);

    result->compressionType = AV_CODEC_ID_H265;
    result->width = m_context.width;
    result->height = m_context.height;

    // Check all nal units for key frame slice
    if (isKeyFrame((const uint8_t*)result->data(), result->dataSize()))
    {
        result->flags |= QnAbstractMediaData::MediaFlags_AVKey;
        auto codecParameters = QnFfmpegHelper::createVideoCodecParametersMp4(
            result.get(), result->width, result->height);
    if (codecParameters && (!m_codecParameters || !m_codecParameters->isEqual(*codecParameters)))
        m_codecParameters = codecParameters;
    }
    result->context = m_codecParameters;
    result->timestamp = rtpTime;
    return result;
}

void HevcParser::clear()
{
    reset();
}

void HevcParser::reset(bool softReset)
{
    if (m_context.keyDataFound || !softReset)
    {
        m_context.inStreamVpsFound = false;
        m_context.inStreamSpsFound = false;
        m_context.inStreamPpsFound = false;
    }
    if (!softReset)
        m_frameStartFound = false;

    m_context.keyDataFound = false;
    m_chunks.clear();
}

bool HevcParser::extractPictureDimensionsFromSps(const uint8_t* buffer, int bufferLength)
{
    hevc::SequenceParameterSet sps;
    if (!nx::media::hevc::parseNalUnit(buffer, bufferLength, sps))
        return false;

    m_context.width = sps.width;
    m_context.height = sps.height;

    return true;
}

void HevcParser::insertPayloadHeader(
    uint8_t** payloadStart,
    int* payloadLength,
    nx::media::hevc::NalUnitType unitType,
    uint8_t tid)
{
    *payloadStart -= hevc::NalUnitHeader::kTotalLength;
    *payloadLength += hevc::NalUnitHeader::kTotalLength;
    (*payloadStart)[0] = ((uint8_t)unitType) << 1;
    (*payloadStart)[1] = tid;
}

nx::utils::ByteArray HevcParser::buildParameterSetsIfNeeded()
{
    constexpr int kHeaderReservedSize = 64;
    nx::utils::ByteArray buffer(/*aligment*/ 1, kHeaderReservedSize, /*padding*/ 0);
    if (!m_context.inStreamVpsFound && m_context.vps)
    {
        uint32_t sizeData = htonl(m_context.vps->size());
        buffer.write((const char*)&sizeData, sizeof(uint32_t));
        buffer.write(
            m_context.vps->data(),
            m_context.vps->size());
    }
    if (!m_context.inStreamSpsFound && m_context.sps)
    {
        uint32_t sizeData = htonl(m_context.sps->size());
        buffer.write((const char*)&sizeData, sizeof(uint32_t));
        buffer.write(
            m_context.sps->data(),
            m_context.sps->size());
    }
    if (!m_context.inStreamPpsFound && m_context.pps)
    {
        uint32_t sizeData = htonl(m_context.pps->size());
        buffer.write((const char*)&sizeData, sizeof(uint32_t));
        buffer.write(
            m_context.pps->data(),
            m_context.pps->size());
    }
    return buffer;
}

} // namespace nx::rtp
