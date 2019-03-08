#include "h264_rtp_parser.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/rtp/rtp.h>
#include <nx/utils/log/log.h>

namespace nx::streaming::rtp {

static const char H264_NAL_PREFIX[4] = {0x00, 0x00, 0x00, 0x01};
static const char H264_NAL_SHORT_PREFIX[3] = {0x00, 0x00, 0x01};
static const int kMinIdrCountToDetectIFrameByIdr = 2;

H264Parser::H264Parser():
        VideoStreamParser(),
        m_spsInitialized(false),
        m_rtpChannel(98),
        m_prevSequenceNum(-1),
        m_builtinSpsFound(false),
        m_builtinPpsFound(false),
        m_keyDataExists(false),
        m_idrCounter(0),
        m_frameExists(false),
        m_firstSeqNum(0),
        m_packetPerNal(0),
        m_videoFrameSize(0),
        m_lastRtpTime(0)
{
    StreamParser::setFrequency(90000);
}

void H264Parser::setSdpInfo(const Sdp::Media& sdp)
{
    if (sdp.rtpmap.clockRate > 0)
        StreamParser::setFrequency(sdp.rtpmap.clockRate);
    m_rtpChannel = sdp.payloadType;
    for (const QString& param: sdp.fmtp.params)
    {
        if (param.startsWith("sprop-parameter-sets"))
        {
            int pos = param.indexOf('=');
            if (pos >= 0)
            {
                QString h264SpsPps = param.mid(pos+1);
                QStringList nalUnits = h264SpsPps.split(',');
                for (QString nalStr: nalUnits)
                {
                    QByteArray nal = QByteArray::fromBase64(nalStr.toUtf8());
                    // some cameras( Digitalwatchdog sends extra start code in SPSPSS sdp string );
                    nal = NALUnit::dropBorderedStartCodes(nal);
                    if( nal.size() > 0 && (nal[0] & 0x1f) == nuSPS )
                        decodeSpsInfo( nal );

                    m_sdpSpsPps << QByteArray(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX)).append(nal);
                }
            }
        }
    }
}

int H264Parser::getSpsPpsSize() const
{
    int result = 0;
    for (int i = 0; i < m_sdpSpsPps.size(); ++i)
        result += m_sdpSpsPps[i].size();
    return result;
}

void H264Parser::serializeSpsPps(QnByteArray& dst)
{
    for (int i = 0; i < m_sdpSpsPps.size(); ++i)
        dst.uncheckedWrite(m_sdpSpsPps[i].data(), m_sdpSpsPps[i].size());
}

void H264Parser::decodeSpsInfo(const QByteArray& data)
{
    try
    {
        m_sps.decodeBuffer(
            (const quint8*) data.constData(),
            (const quint8*) data.constData() + data.size());
        m_sps.deserialize();
        m_spsInitialized = true;

    } catch(BitStreamException& e)
    {
        qWarning() << "Can't deserialize SPS unit. Bitstream error" << e.what();
    }
}

bool H264Parser::isBufferOverflow() const
{
    int addHeaderSize = 0;
    if (m_keyDataExists && (!m_builtinSpsFound || !m_builtinPpsFound))
        addHeaderSize = getSpsPpsSize();

    int totalSize = m_videoFrameSize + addHeaderSize;
    return totalSize > (int) MAX_ALLOWED_FRAME_SIZE;
}

QnCompressedVideoDataPtr H264Parser::createVideoData(const quint8* rtpBuffer, quint32 rtpTime)
{
    int addHeaderSize = 0;
    if (m_keyDataExists && (!m_builtinSpsFound || !m_builtinPpsFound))
        addHeaderSize = getSpsPpsSize();

    int totalSize = m_videoFrameSize + addHeaderSize;

    QnWritableCompressedVideoDataPtr result =
        QnWritableCompressedVideoDataPtr(
            new QnWritableCompressedVideoData(
                CL_MEDIA_ALIGNMENT,
                totalSize));
    result->compressionType = AV_CODEC_ID_H264;
    result->width = m_spsInitialized ? m_sps.getWidth() : -1;
    result->height = m_spsInitialized ? m_sps.getHeight() : -1;
    if (m_keyDataExists)
    {
        result->flags = QnAbstractMediaData::MediaFlags_AVKey;
        if (!m_builtinSpsFound || !m_builtinPpsFound)
            serializeSpsPps(result->m_data);
    }

    size_t spsNaluStartOffset = (size_t) -1;
    size_t spsNaluSize = 0;
    for (size_t i = 0; i < m_chunks.size(); ++i)
    {
        auto buffer = m_chunks[i].bufferStart ? m_chunks[i].bufferStart : rtpBuffer;
        if (m_chunks[i].nalStart)
        {
            // Close previous SPS.
            if ((spsNaluStartOffset != (size_t)-1) && (spsNaluSize == 0))
                spsNaluSize = result->m_data.size() - spsNaluStartOffset;

            result->m_data.uncheckedWrite(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));

            auto nalType = *(buffer + m_chunks[i].bufferOffset) & 0x1f;
            if ((m_chunks[i].len > 0) && (nalType == nuSPS))
            {
                // More than one SPS is not supported, currently we analyze only the last one.
                spsNaluStartOffset = result->m_data.size();
                spsNaluSize = 0;
            }
        }

        result->m_data.uncheckedWrite(
            (const char*) buffer + m_chunks[i].bufferOffset, m_chunks[i].len);
    }

    if (spsNaluStartOffset != (size_t) -1)
    {
        if (spsNaluSize == 0)
            spsNaluSize = result->m_data.size() - spsNaluStartOffset;

        // Decoding sps to detect stream resolution change.
        decodeSpsInfo(QByteArray::fromRawData(
            result->m_data.constData() + spsNaluStartOffset, (int) spsNaluSize));
    }

    result->timestamp = rtpTime;
    clearInternalBuffer();
    return result;
}

bool H264Parser::clearInternalBuffer()
{
    m_chunks.clear();
    m_videoFrameSize = 0;
    m_keyDataExists
        = m_builtinPpsFound
        = m_builtinSpsFound
        = false;
    m_frameExists = false;
    m_packetPerNal = 0;
    return false;
}

bool H264Parser::isFirstSliceNal(
    const quint8 nalType,
    const quint8* data,
    int dataLen ) const
{
    bool isSlice = NALUnit::isSliceNal(nalType);
    if(!isSlice)
        return false;

    BitStreamReader bitReader;
    int macroNum = -1;

    try
    {
        bitReader.setBuffer(data+1, data + dataLen);
        macroNum = NALUnit::extractUEGolombCode(bitReader);
    }
    catch(BitStreamException&)
    {
        return false;
    }

    return macroNum == 0;
}

void H264Parser::updateNalFlags(const quint8* data, int dataLen)
{
    const quint8* dataEnd = data + dataLen;
    while (data < dataEnd)
    {
        int nalUnitType = *data & 0x1f;

        if (nalUnitType == nuSPS)
            m_builtinSpsFound = true;
        else if (nalUnitType == nuPPS)
            m_builtinPpsFound = true;
        else if (NALUnit::isSliceNal(nalUnitType))
        {
            m_frameExists = true;
            if (nalUnitType == nuSliceIDR)
            {
                m_keyDataExists = true;
                if (m_idrCounter < kMinIdrCountToDetectIFrameByIdr)
                {
                    if (isFirstSliceNal(nalUnitType, data, dataLen))
                        ++m_idrCounter;
                }
            }
            else if (m_idrCounter < kMinIdrCountToDetectIFrameByIdr && NALUnit::isIFrame(data, dataLen))
            {
                m_keyDataExists = true;
            }
            break; //< optimization
        }

        data = NALUnit::findNextNAL(data, dataEnd);
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

    if (packetType == STAP_A_PACKET)
    {
        if (curPtr + 2 >= bufferEnd)
            return false;

        curPtr += 2;
    }
    else if (packetType == STAP_B_PACKET)
    {
        if (curPtr + 4 >= bufferEnd)
            return false;

        curPtr += 4;
    }
    else if (packetType == MTAP16_PACKET || packetType == MTAP24_PACKET)
    {
        return false;
    }
    else if (packetType == FU_A_PACKET || packetType == FU_B_PACKET)
    {
        if (!(*curPtr & 0x80))
            return false; //< FU_A/B continue packet
    }
    else
    {
        curPtr--;
    }

    auto nalUnitType = *curPtr & 0x1f;
    return !NALUnit::isSliceNal(nalUnitType) || isFirstSliceNal(nalUnitType, curPtr, nalLen);
}

bool H264Parser::processData(
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    bool& gotData)
{
    gotData = false;
    int nalUnitLen;
    quint8 nalUnitType;

    quint8* rtpBuffer = rtpBufferBase + bufferOffset;

    if (bytesRead < RtpHeader::kSize + 1)
        return clearInternalBuffer();

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    quint8* curPtr = rtpBuffer + RtpHeader::kSize;
    if (rtpHeader->extension)
    {
        if (bytesRead < RtpHeader::kSize + 4)
            return clearInternalBuffer();

        int extWords = ((int(curPtr[2]) << 8) + curPtr[3]);
        curPtr += extWords*4 + 4;
    }

    const quint8* bufferEnd = rtpBuffer + bytesRead;
    quint16 sequenceNum = ntohs(rtpHeader->sequence);

    if (curPtr >= bufferEnd)
        return clearInternalBuffer();

    if (rtpHeader->payloadType != m_rtpChannel)
        return true; //< Skip data.

    bool isPacketLost =
        m_prevSequenceNum != -1 &&
        quint16(m_prevSequenceNum) != quint16(sequenceNum-1) &&
        m_prevSequenceNum != sequenceNum; // Some cameras send duplicate packets, ignore it

    if (m_videoFrameSize > (int) MAX_ALLOWED_FRAME_SIZE)
    {
        NX_WARNING(this, "Too large RTP/H.264 frame. Truncate video buffer");
        clearInternalBuffer();
        isPacketLost = true;
    }

    if (isPacketLost)
        emit packetLostDetected(m_prevSequenceNum, sequenceNum);

    m_prevSequenceNum = sequenceNum;
    if (isPacketLost)
        return false;

    if (rtpHeader->padding)
    {
        bufferEnd -= bufferEnd[-1];
        if (curPtr >= bufferEnd)
            return clearInternalBuffer();
    }

    if (isPacketStartsNewFrame(curPtr, bufferEnd))
    {
        m_mediaData = createVideoData(rtpBufferBase, m_lastRtpTime); // last packet
        gotData = true;
    }
    m_lastRtpTime = ntohl(rtpHeader->timestamp);


    m_packetPerNal++;

    int packetType = *curPtr & 0x1f;
    int nalRefIDC = *curPtr++ & 0xe0;

    switch (packetType)
    {
        case STAP_B_PACKET:
        {
            if (bufferEnd-curPtr < 2)
                return clearInternalBuffer();
            curPtr += 2;
            // TODO: #lbusygin fallthrough here, is it deliberately?
        }
        case STAP_A_PACKET:
        {
            while (curPtr < bufferEnd)
            {
                if (bufferEnd-curPtr < 2)
                    return clearInternalBuffer();
                nalUnitLen = (curPtr[0] << 8) + curPtr[1];
                curPtr += 2;
                if (bufferEnd-curPtr < nalUnitLen)
                    return clearInternalBuffer();

                nalUnitType = *curPtr & 0x1f;
                m_chunks.emplace_back((int) (curPtr - rtpBufferBase), nalUnitLen, true);
                m_videoFrameSize += nalUnitLen + 4;
                updateNalFlags(curPtr, bufferEnd - curPtr);
                curPtr += nalUnitLen;
            }
            break;
        }
        case FU_B_PACKET:
        case FU_A_PACKET:
        {
            if (bufferEnd-curPtr < 1)
                return clearInternalBuffer();
            nalUnitType = *curPtr & 0x1f;
            updateNalFlags(curPtr, bufferEnd - curPtr);
            if (*curPtr  & 0x80) // FU_A first packet
            {
                m_firstSeqNum = sequenceNum;
                m_packetPerNal = 0;
                nalUnitType |= nalRefIDC;
            }
            else
            {
                // if packet loss occured in the middle of FU packet, reset flag.
                // packet loss will be reported on the last FU packet. So, do not report problem twice
                isPacketLost = false;
            }

            if (*curPtr  & 0x40)
            {
                // FU_A last packet
                if (quint16(sequenceNum - m_firstSeqNum) != m_packetPerNal)
                    return clearInternalBuffer(); // packet loss detected
            }

            curPtr++;
            if (packetType == FU_B_PACKET)
            {
                if (bufferEnd-curPtr < 2)
                    return clearInternalBuffer();
                curPtr += 2;

            }
            if (m_packetPerNal == 0)
            {
                // FU_A first packetf
                --curPtr;
                *curPtr = nalUnitType;
            }
            m_chunks.emplace_back(
                (int) (curPtr - rtpBufferBase),
                (quint16) (bufferEnd - curPtr),
                m_packetPerNal == 0);
            m_videoFrameSize += bufferEnd - curPtr + (m_packetPerNal == 0 ? 4 : 0);
            break;
        }
        case MTAP16_PACKET:
        case MTAP24_PACKET:
        {
            // Not implemented
            NX_WARNING(this, "Got MTAP packet. Not implemented yet");
            return clearInternalBuffer();
        }
        default:
        {
            curPtr--;
            nalUnitType = *curPtr & 0x1f;
            m_chunks.emplace_back(
                (int) (curPtr - rtpBufferBase), (quint16) (bufferEnd - curPtr), true);
            m_videoFrameSize += bufferEnd - curPtr + 4;
            updateNalFlags(curPtr, bufferEnd - curPtr);
            break; // ignore unknown data
        }
    }

    if (isPacketLost && !m_keyDataExists)
        return clearInternalBuffer();

    if (rtpHeader->marker && m_frameExists && !gotData)
    {
        m_mediaData = createVideoData(rtpBufferBase, m_lastRtpTime); // last packet
        gotData = true;
    }

    if (gotData)
    {
        backupCurrentData(rtpBufferBase);
    }
    else if (isBufferOverflow())
    {
        NX_WARNING(this, "RTP parser buffer overflow");
        emit packetLostDetected(0, 0);
        return false;
    }

    return true;
}

} // namespace nx::streaming::rtp

#endif // ENABLE_DATA_PROVIDERS
