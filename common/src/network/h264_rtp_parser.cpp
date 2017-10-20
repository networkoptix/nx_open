#include "h264_rtp_parser.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/rtp_stream_parser.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/streaming/config.h>
#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>

static const char H264_NAL_PREFIX[4] = {0x00, 0x00, 0x00, 0x01};
static const char H264_NAL_SHORT_PREFIX[3] = {0x00, 0x00, 0x01};
static const int kMinIdrCountToDetectIFrameByIdr = 2;

CLH264RtpParser::CLH264RtpParser():
        QnRtpVideoStreamParser(),
        m_spsInitialized(false),
        m_frequency(90000), // default value
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
        m_previousPacketHasMarkerBit(false),
        m_lastRtpTime(0)
{
}

CLH264RtpParser::~CLH264RtpParser()
{
}

void CLH264RtpParser::setSDPInfo(QList<QByteArray> lines)
{
    for (int i = 0; i < lines.size(); ++ i)
    {
        lines[i] = lines[i].trimmed();
        if (lines[i].startsWith("a=rtpmap:"))
        {
            QList<QByteArray> values = lines[i].split(' ');
            if (values.size() < 2)
                continue;
            QByteArray codecName = values[1];
            if (!codecName.toUpper().startsWith("H264"))
                continue;
            QList<QByteArray> values2 = codecName.split('/');
            if (values2.size() < 2)
                continue;
            m_frequency = values2[1].toUInt();

            values = values[0].split(':');
            if (values.size() < 2)
                continue;
            m_rtpChannel = values[1].toUInt();
        }
    }

    for (int i = 0; i < lines.size(); ++ i)
    {
        lines[i] = lines[i].trimmed();
        if (lines[i].startsWith("a=fmtp:"))
        {
            int valueIndex = lines[i].indexOf(' ');
            if (valueIndex == -1)
                continue;

            QList<QByteArray> fmpParam = lines[i].left(valueIndex).split(':'); //values[0].split(':');
            if (fmpParam.size() < 2 || fmpParam[1].toUInt() != (uint)m_rtpChannel)
                continue;

            QList<QByteArray> h264Params = lines[i].mid(valueIndex+1).split(';');
            for (int i = 0; i < h264Params.size(); ++i)
            {
                QByteArray h264Parma = h264Params[i].trimmed();
                if (h264Parma.startsWith("sprop-parameter-sets"))
                {
                    int pos = h264Parma.indexOf('=');
                    if (pos >= 0)
                    {
                        QByteArray h264SpsPps = h264Parma.mid(pos+1);
                        QList<QByteArray> nalUnits = h264SpsPps.split(',');
                        for(QByteArray nal: nalUnits)
                        {
                            nal = QByteArray::fromBase64(nal);

                            {
                                // some cameras( Digitalwatchdog sends extra start code in SPSPSS sdp string );
                                QByteArray startCode(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
                                QByteArray startCodeShort(H264_NAL_SHORT_PREFIX, sizeof(H264_NAL_SHORT_PREFIX));

                                if (nal.endsWith(startCode))
                                    nal.remove(nal.size()-4,4);
                                else if (nal.endsWith(startCodeShort))
                                    nal.remove(nal.size()-3,3);


                                if (nal.startsWith(startCode))
                                    nal.remove(0,4);
                                else if (nal.startsWith(startCodeShort))
                                    nal.remove(0,3);
                            }
                            if( nal.size() > 0 && (nal[0] & 0x1f) == nuSPS )
                                decodeSpsInfo( nal );

                            m_sdpSpsPps << QByteArray(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX)).append(nal);
                        }
                    }
                }
            }
        }
    }
}

int CLH264RtpParser::getSpsPpsSize() const
{
    int result = 0;
    for (int i = 0; i < m_sdpSpsPps.size(); ++i)
        result += m_sdpSpsPps[i].size();
    return result;
}

void CLH264RtpParser::backupCurrentData(quint8* bufferStart)
{
    size_t chunksLength = 0;
    for (const auto& chunk: m_chunks)
        chunksLength += chunk.len;

    m_nextFrameChunksBuffer.resize(chunksLength);

    size_t offset = 0;
    quint8* nextFrameBufRaw = m_nextFrameChunksBuffer.data();
    for (auto& chunk: m_chunks)
    {
        memcpy(nextFrameBufRaw + offset, bufferStart + chunk.bufferOffset, chunk.len);
        chunk.bufferStart = nextFrameBufRaw;
        chunk.bufferOffset = offset;
        offset += chunk.len;
    }
}
void CLH264RtpParser::serializeSpsPps(QnByteArray& dst)
{
    for (int i = 0; i < m_sdpSpsPps.size(); ++i)
        dst.uncheckedWrite(m_sdpSpsPps[i].data(), m_sdpSpsPps[i].size());
}

void CLH264RtpParser::decodeSpsInfo(const QByteArray& data)
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

bool CLH264RtpParser::isBufferOverflow() const
{
    int addHeaderSize = 0;
    if (m_keyDataExists && (!m_builtinSpsFound || !m_builtinPpsFound))
        addHeaderSize = getSpsPpsSize();

    int totalSize = m_videoFrameSize + addHeaderSize;
    return totalSize > (int) MAX_ALLOWED_FRAME_SIZE;
}

QnCompressedVideoDataPtr CLH264RtpParser::createVideoData(
    const quint8            *rtpBuffer,
    quint32                 rtpTime,
    const QnRtspStatistic   &statistics
)
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
    if (m_keyDataExists) {
        result->flags = QnAbstractMediaData::MediaFlags_AVKey;
        if (!m_builtinSpsFound || !m_builtinPpsFound)
            serializeSpsPps(result->m_data);
    }

    size_t spsNaluStartOffset = (size_t)-1;
    size_t spsNaluSize = 0;
    for (size_t i = 0; i < m_chunks.size(); ++i)
    {

        auto buffer = m_chunks[i].bufferStart ?
            m_chunks[i].bufferStart : rtpBuffer;

        if (m_chunks[i].nalStart)
        {
            if( (spsNaluStartOffset != (size_t)-1) && (spsNaluSize == 0) )
                spsNaluSize = result->m_data.size() - spsNaluStartOffset;

            result->m_data.uncheckedWrite(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));

            auto nalType = (
                *( buffer + m_chunks[i].bufferOffset ) & 0x1f);

            if( (m_chunks[i].len > 0) && (nalType == nuSPS) )
                spsNaluStartOffset = result->m_data.size();
        }

        result->m_data.uncheckedWrite(
            (const char*) buffer + m_chunks[i].bufferOffset, m_chunks[i].len);
    }

    if( (spsNaluStartOffset != (size_t)-1) )
    {
        //decoding sps to detect stream resolution change
        decodeSpsInfo( QByteArray::fromRawData( result->m_data.constData() + spsNaluStartOffset, spsNaluSize ) );
    }

    if (m_timeHelper) {
        result->timestamp = m_timeHelper->getUsecTime(rtpTime, statistics, m_frequency);
#if 0
        qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch()*1000;
        if (qAbs(currentTime - result->timestamp) > 500*1000) {
            qDebug()
                << "large RTSP video jitter "
                << (result->timestamp - currentTime)/1000
                << "RtpTime="
                << rtpTime;
        }
#endif
    }
    else
        result->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    clearInternalBuffer();
    return result;
}

bool CLH264RtpParser::clearInternalBuffer()
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

bool CLH264RtpParser::isIFrame(const quint8* data, int dataLen) const
{
    if (dataLen < 2)
        return false;

    quint8 nalType = *data & 0x1f;
    bool isSlice = isSliceNal(nalType);
    if (!isSlice)
        return false;

    if (nalType == nuSliceIDR)
        return true;

    BitStreamReader bitReader;
    bitReader.setBuffer(data+1, data + dataLen);
    try
    {
        //extract first_mb_in_slice
        NALUnit::extractUEGolombCode(bitReader);

        int slice_type = NALUnit::extractUEGolombCode(bitReader);
        if (slice_type >= 5)
            slice_type -= 5; // +5 flag is: all other slice at this picture must be same type

        return (slice_type == SliceUnit::I_TYPE || slice_type == SliceUnit::SI_TYPE);
    }
    catch (...)
    {
        return false;
    }
}

bool CLH264RtpParser::isFirstSliceNal(
    const quint8 nalType,
    const quint8* data,
    int dataLen ) const
{
    bool isSlice = isSliceNal(nalType);
    if(!isSlice)
        return false;

    BitStreamReader bitReader;
    int macroNum = -1;

    try
    {
        bitReader.setBuffer(data+1, data + dataLen);
        macroNum = NALUnit::extractUEGolombCode(bitReader);
    }
    catch(BitStreamException &e)
    {
        return false;
    }

    return macroNum == 0;
}

void CLH264RtpParser::updateNalFlags(const quint8* data, int dataLen)
{
    const quint8* dataEnd = data + dataLen;
    while (data < dataEnd)
    {
        int nalUnitType = *data & 0x1f;

        if (nalUnitType == nuSPS)
            m_builtinSpsFound = true;
        else if (nalUnitType == nuPPS)
            m_builtinPpsFound = true;
        else if (isSliceNal(nalUnitType))
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
            else if (m_idrCounter < kMinIdrCountToDetectIFrameByIdr && isIFrame(data, dataLen))
            {
                m_keyDataExists = true;
            }
            break; //< optimization
        }

        data = NALUnit::findNextNAL(data, dataEnd);
    }
}

bool CLH264RtpParser::isPacketStartsNewFrame(
    const quint8* curPtr,
    const quint8* bufferEnd) const
{
    int packetType = *curPtr++ & 0x1f;

    if (!m_frameExists)
        return false; //< no slice found so far. no need to create frame

    if (m_previousPacketHasMarkerBit)
        return true;

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
    return !isSliceNal(nalUnitType) || isFirstSliceNal(nalUnitType, curPtr, nalLen);
}

bool CLH264RtpParser::isSliceNal(quint8 nalUnitType) const
{
    return nalUnitType >= nuSliceNonIDR && nalUnitType <= nuSliceIDR;
}

bool CLH264RtpParser::processData(
    quint8* rtpBufferBase,
    int bufferOffset,
    int bytesRead,
    const QnRtspStatistic& statistics,
    bool& gotData)
{
    gotData = false;
    int nalUnitLen;
    quint8 nalUnitType;

    quint8* rtpBuffer = rtpBufferBase + bufferOffset;

    if (bytesRead < RtpHeader::RTP_HEADER_SIZE + 1)
        return clearInternalBuffer();

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
    if (rtpHeader->extension)
    {
        if (bytesRead < RtpHeader::RTP_HEADER_SIZE + 4)
            return clearInternalBuffer();

        int extWords = ((int(curPtr[2]) << 8) + curPtr[3]);
        curPtr += extWords*4 + 4;
    }

    const quint8* bufferEnd = rtpBuffer + bytesRead;
    quint16 sequenceNum = ntohs(rtpHeader->sequence);

    if (curPtr >= bufferEnd)
        return clearInternalBuffer();

    bool isPacketLost = m_prevSequenceNum != -1
        && quint16(m_prevSequenceNum) != quint16(sequenceNum-1);

    if (m_videoFrameSize > (int) MAX_ALLOWED_FRAME_SIZE)
    {
        NX_LOG("Too large RTP/H.264 frame. Truncate video buffer", cl_logWARNING);
        clearInternalBuffer();
        isPacketLost = true;
    }

    auto processPacketLost = [this, sequenceNum]()
    {
        if (m_timeHelper) {
            NX_LOG(QString(
                lit("RTP Packet loss detected for camera %1. Old seq=%2, new seq=%3"))
                .arg(m_timeHelper->getResID())
                .arg(m_prevSequenceNum)
                .arg(sequenceNum), cl_logWARNING);
        }
        else {
            NX_LOG("RTP Packet loss detected!!!!", cl_logWARNING);
        }
        clearInternalBuffer();
        emit packetLostDetected(m_prevSequenceNum, sequenceNum);
    };

    if (isPacketLost)
        processPacketLost();

    m_prevSequenceNum = sequenceNum;
    if (isPacketLost)
        return false;

    if (rtpHeader->payloadType != m_rtpChannel)
        return true; // skip data

    if (rtpHeader->padding)
    {
        bufferEnd -= bufferEnd[-1];
        if (curPtr >= bufferEnd)
            return clearInternalBuffer();
    }

    if (isPacketStartsNewFrame(curPtr, bufferEnd))
    {
        m_mediaData = createVideoData(
            rtpBufferBase,
            m_lastRtpTime,
            statistics
            ); // last packet
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
                m_chunks.emplace_back(curPtr-rtpBufferBase, nalUnitLen, true);
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
                (int) (curPtr-rtpBufferBase), bufferEnd - curPtr, m_packetPerNal == 0);
            m_videoFrameSize += bufferEnd - curPtr + (m_packetPerNal == 0 ? 4 : 0);
            break;
        }
        case MTAP16_PACKET:
        case MTAP24_PACKET:
        {
            // Not implemented
            NX_LOG("Got MTAP packet. Not implemented yet", cl_logWARNING);
            return clearInternalBuffer();
        }
        default:
        {
            curPtr--;
            nalUnitType = *curPtr & 0x1f;
            m_chunks.emplace_back(curPtr-rtpBufferBase, bufferEnd - curPtr, true);
            m_videoFrameSize += bufferEnd - curPtr + 4;
            updateNalFlags(curPtr, bufferEnd - curPtr);
            break; // ignore unknown data
        }
    }

    if (isPacketLost && !m_keyDataExists)
        return clearInternalBuffer();
    m_previousPacketHasMarkerBit = rtpHeader->marker;

    if (gotData)
        backupCurrentData(rtpBufferBase);
    else if (isBufferOverflow())
    {
        processPacketLost();
        return false;
    }

    return true;
}

#endif // ENABLE_DATA_PROVIDERS
