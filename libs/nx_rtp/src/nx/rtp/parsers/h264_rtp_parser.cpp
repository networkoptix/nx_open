// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "h264_rtp_parser.h"

#include <nx/codec/h264/common.h>
#include <nx/codec/h264/slice_header.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/rtp/rtp.h>
#include <nx/utils/log/log.h>

namespace nx::rtp {

static const int kMinIdrCountToDetectIFrameByIdr = 2;

H264Parser::H264Parser():
    VideoStreamParser(),
    m_spsInitialized(false),
    m_builtinSpsFound(false),
    m_builtinPpsFound(false),
    m_keyDataExists(false),
    m_idrCounter(0),
    m_frameExists(false),
    m_firstSeqNum(0),
    m_packetPerNal(0),
    m_lastRtpTime(0)
{
    StreamParser::setFrequency(90'000);
}

void H264Parser::setSdpInfo(const Sdp::Media& sdp)
{
    constexpr int kHeaderReservedSize = 64;
    if (sdp.rtpmap.clockRate > 0)
        StreamParser::setFrequency(sdp.rtpmap.clockRate);
    for (const QString& param: sdp.fmtp.params)
    {
        if (param.startsWith("sprop-parameter-sets"))
        {
            int pos = param.indexOf('=');
            if (pos >= 0)
            {
                QString h264SpsPps = param.mid(pos+1);
                QStringList nalUnits = h264SpsPps.split(',');
                m_sdpSpsPps.reserve(kHeaderReservedSize);
                for (QString nalStr: nalUnits)
                {
                    QByteArray nal = QByteArray::fromBase64(nalStr.toUtf8());
                    // Some cameras sends extra start code in SPSPSS sdp string.
                    nal = nx::media::nal::dropBorderedStartCodes(nal);
                    if (nal.size() > 0 && nx::media::h264::decodeType(nal[0]) == nx::media::h264::nuSPS)
                        decodeSpsInfo((uint8_t*)nal.data(), nal.size());

                    uint32_t sizeData = htonl(nal.size());
                    m_sdpSpsPps.insert(m_sdpSpsPps.end(),
                        (uint8_t*) &sizeData,
                        (uint8_t*) &sizeData + sizeof(uint32_t));
                    m_sdpSpsPps.insert(m_sdpSpsPps.end(),
                        (uint8_t*) nal.data(),
                        (uint8_t*) nal.data() + nal.size());
                }
            }
        }
    }
}

void H264Parser::decodeSpsInfo(const uint8_t* data, int size)
{
    try
    {
        m_sps.decodeBuffer(data, data + size);
        m_sps.deserialize();
        m_spsInitialized = true;

    }
    catch(nx::utils::BitStreamException& e)
    {
        NX_WARNING(this, "%1: Can't deserialize SPS unit. Bitstream error: %2", logId(), e.what());
    }
}

bool H264Parser::isBufferOverflow() const
{
    int addHeaderSize = 0;
    if (m_keyDataExists && (!m_builtinSpsFound || !m_builtinPpsFound))
        addHeaderSize = m_sdpSpsPps.size();

    int totalSize = m_chunks.size() + addHeaderSize;
    return totalSize > MAX_ALLOWED_FRAME_SIZE;
}

QnCompressedVideoDataPtr H264Parser::createVideoData(const quint8* rtpBuffer, quint32 rtpTime)
{
    QnWritableCompressedVideoDataPtr result;
    if (m_keyDataExists && (!m_builtinSpsFound || !m_builtinPpsFound))
        result = m_chunks.buildFrame(rtpBuffer, m_sdpSpsPps.data(), m_sdpSpsPps.size());
    else
        result = m_chunks.buildFrame(rtpBuffer, nullptr, 0);

    result->compressionType = AV_CODEC_ID_H264;
    result->timestamp = rtpTime;
    result->width = m_spsInitialized ? m_sps.getWidth() : -1;
    result->height = m_spsInitialized ? m_sps.getHeight() : -1;

    if (m_keyDataExists)
    {
        result->flags = QnAbstractMediaData::MediaFlags_AVKey;
        auto codecParameters = QnFfmpegHelper::createVideoCodecParametersMp4(
            result.get(), result->width, result->height);
        if (codecParameters && (!m_codecParameters || !m_codecParameters->isEqual(*codecParameters)))
            m_codecParameters = codecParameters;
    }


    result->context = m_codecParameters;

    clear();
    return result;
}

void H264Parser::clear()
{
    m_chunks.clear();
    m_keyDataExists = false;
    m_builtinPpsFound = false;
    m_builtinSpsFound = false;
    m_frameExists = false;
    m_packetPerNal = 0;
}

bool H264Parser::isFirstSliceNal(
    const quint8 nalType,
    const quint8* data,
    int dataLen ) const
{
    bool isSlice = nx::media::h264::isSliceNal(nalType);
    if(!isSlice)
        return false;

    nx::utils::BitStreamReader bitReader;
    int macroNum = -1;

    try
    {
        bitReader.setBuffer(data+1, data + dataLen);
        macroNum = bitReader.getGolomb();
    }
    catch(nx::utils::BitStreamException&)
    {
        return false;
    }

    return macroNum == 0;
}

void H264Parser::updateNalFlags(const uint8_t* data, int size)
{
    const auto nalUnitType = nx::media::h264::decodeType(*data);

    if (nalUnitType == nx::media::h264::nuSPS)
    {
        m_builtinSpsFound = true;
        decodeSpsInfo(data, size);
    }
    else if (nalUnitType == nx::media::h264::nuPPS)
    {
        m_builtinPpsFound = true;
    }
    else if (nx::media::h264::isSliceNal(nalUnitType))
    {
        m_frameExists = true;
        if (nalUnitType == nx::media::h264::nuSliceIDR)
        {
            m_keyDataExists = true;
            if (m_idrCounter < kMinIdrCountToDetectIFrameByIdr)
            {
                if (isFirstSliceNal(nalUnitType, data, size))
                    ++m_idrCounter;
            }
        }
        else if (m_idrCounter < kMinIdrCountToDetectIFrameByIdr && nx::media::h264::isIFrame(data, size))
        {
            m_keyDataExists = true;
        }
    }
}

bool H264Parser::isPacketStartsNewFrame(
    const quint8* curPtr,
    const quint8* bufferEnd) const
{
    int packetType = *curPtr++ & 0x1f;

    if (!m_frameExists)
        return false; //< no slice found so far. no need to create frame

    int nalLen = bufferEnd - curPtr;

    if (packetType == nx::media::h264::STAP_A_PACKET)
    {
        if (curPtr + 2 >= bufferEnd)
            return false;

        curPtr += 2;
    }
    else if (packetType == nx::media::h264::STAP_B_PACKET)
    {
        if (curPtr + 4 >= bufferEnd)
            return false;

        curPtr += 4;
    }
    else if (packetType == nx::media::h264::MTAP16_PACKET || packetType == nx::media::h264::MTAP24_PACKET)
    {
        return false;
    }
    else if (packetType == nx::media::h264::FU_A_PACKET || packetType == nx::media::h264::FU_B_PACKET)
    {
        if (!(*curPtr & 0x80))
            return false; //< FU_A/B continue packet
    }
    else
    {
        curPtr--;
    }

    const auto nalUnitType = nx::media::h264::decodeType(*curPtr);
    if (!nx::media::h264::isSliceNal(nalUnitType))
        return true;
    if (!isFirstSliceNal(nalUnitType, curPtr, nalLen))
        return false;
    if (m_spsInitialized && !m_sps.frame_mbs_only_flag)
    {
        // Interlaced video.
        nx::media::h264::SliceHeader slice;
        slice.decodeBuffer(curPtr, bufferEnd);
        slice.deserialize(&m_sps, nullptr /*pps*/);
        if (slice.bottom_field_flag)
            return false; //< Bottom field of the video frame.
    }
    return true;
}

Result H264Parser::processData(
    const RtpHeader& rtpHeader,
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    bool& gotData)
{
    gotData = false;
    int nalUnitLen;

    quint8* rtpBuffer = rtpBufferBase + bufferOffset;
    quint8* curPtr = rtpBuffer;
    const quint8* bufferEnd = rtpBuffer + bytesRead;
    quint16 sequenceNum = rtpHeader.getSequence();

    if (m_chunks.size() > MAX_ALLOWED_FRAME_SIZE)
    {
        clear();
        return {false, "Too large RTP/H.264 frame. Truncate video buffer"};
    }

    if (isPacketStartsNewFrame(curPtr, bufferEnd))
    {
        m_mediaData = createVideoData(rtpBufferBase, m_lastRtpTime); // last packet
        gotData = true;
    }
    m_lastRtpTime = rtpHeader.getTimestamp();

    m_packetPerNal++;
    uint8_t packetType = nx::media::h264::decodeType(*curPtr);
    int nalRefIDC = *curPtr++ & 0xe0;

    switch (packetType)
    {
        case nx::media::h264::STAP_B_PACKET:
        case nx::media::h264::STAP_A_PACKET:
        {
            if (packetType == nx::media::h264::STAP_B_PACKET)
            {
                if (bufferEnd-curPtr < 2)
                {
                    clear();
                    return {false, NX_FMT("Failed to parse RTP packet, invalid STAP_B_PACKET")};
                }
                curPtr += 2;
            }
            while (curPtr < bufferEnd)
            {
                if (bufferEnd-curPtr < 2)
                {
                    clear();
                    return {false, NX_FMT("Failed to parse RTP packet, invalid STAP_A_PACKET")};
                }
                nalUnitLen = (curPtr[0] << 8) + curPtr[1];
                curPtr += 2;
                if (bufferEnd-curPtr < nalUnitLen)
                {
                    clear();
                    return {false, NX_FMT(
                        "Failed to parse RTP packet, invalid NAL unit length in STAP_A_PACKET")};
                }

                m_chunks.addChunk((int) (curPtr - rtpBufferBase), nalUnitLen, true);
                updateNalFlags(curPtr, bufferEnd - curPtr);
                curPtr += nalUnitLen;
            }
            break;
        }
        case nx::media::h264::FU_B_PACKET:
        case nx::media::h264::FU_A_PACKET:
        {
            if (bufferEnd-curPtr < 1)
            {
                clear();
                return {false, NX_FMT("Failed to parse RTP packet, invalid FU_A_PACKET")};
            }
            const bool startUnit = *curPtr & 0x80;
            const bool endUnit = *curPtr & 0x40;
            uint8_t nalUnitType = nx::media::h264::decodeType(*curPtr); // FU header.
            bool fixNalHeader = true;
            if (startUnit) // FU_A first packet
            {
                // Some cameras send startcode inside of FU unit, so drop it.
                // ex: 80 00 00 00 01 41 ....
                if (packetType == nx::media::h264::FU_A_PACKET)
                {
                    const int bufferSize = bufferEnd - curPtr;
                    const int startCodeSize = nx::media::nal::isStartCode(curPtr + 1, bufferSize - 1);
                    if (startCodeSize)
                    {
                        fixNalHeader = false;
                        curPtr += startCodeSize + 1;
                    }
                }
                updateNalFlags(curPtr, bufferEnd - curPtr);
                m_firstSeqNum = sequenceNum;
                m_packetPerNal = 0;
                nalUnitType |= nalRefIDC;
            }
            if (endUnit)
            {
                // FU_A last packet
                if (quint16(sequenceNum - m_firstSeqNum) != m_packetPerNal)
                {
                    NX_WARNING(this, "%1: Frame is probably broken, invalid packets per NAL: "
                        "%2, first: %3, current: %4", logId(), m_packetPerNal, m_firstSeqNum, sequenceNum);
                }
            }

            curPtr++;
            if (packetType == nx::media::h264::FU_B_PACKET)
            {
                if (bufferEnd-curPtr < 2)
                {
                    clear();
                    return {false, "Failed to parse RTP packet, invalid FU_B_PACKET"};
                }
                curPtr += 2;

            }
            if (m_packetPerNal == 0)
            {
                // FU_A first packetf
                --curPtr;
                if (fixNalHeader)
                    *curPtr = nalUnitType;
            }
            m_chunks.addChunk(
                (int) (curPtr - rtpBufferBase),
                (quint16) (bufferEnd - curPtr),
                m_packetPerNal == 0);
            break;
        }
        case nx::media::h264::MTAP16_PACKET:
        case nx::media::h264::MTAP24_PACKET:
        {
            // Not implemented.
            clear();
            return {false, "Got MTAP packet. Not implemented yet"};
        }
        default: // Single NAl unit packet.
        {
            curPtr--;
            handleSingleNalunitPacket(
                rtpBufferBase, int(curPtr - rtpBufferBase), int(bufferEnd - curPtr));
        }
    }

    if (rtpHeader.marker && m_frameExists && !gotData)
    {
        m_mediaData = createVideoData(rtpBufferBase, m_lastRtpTime); // last packet
        gotData = true;
    }

    if (gotData)
    {
        m_chunks.backupCurrentData(rtpBufferBase);
    }
    else if (isBufferOverflow())
    {
        clear();
        return {false, "RTP parser buffer overflow"};
    }

    return {true};
}

void H264Parser::handleSingleNalunitPacket(uint8_t* buffer, int offset, int size)
{
    // Check data for other NAL units, some cameras send multiple units in Single NAL unit packet.
    auto nalUnits = nx::media::nal::findNalUnitsAnnexB(buffer + offset, size, true);
    for (const auto& nalu: nalUnits)
    {
        m_chunks.addChunk(nalu.data - buffer, nalu.size, true);
        updateNalFlags(nalu.data, nalu.size);
    }
}

} // namespace nx::rtp
