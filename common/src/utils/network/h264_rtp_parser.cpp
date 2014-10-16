#include "h264_rtp_parser.h"

#ifdef ENABLE_DATA_PROVIDERS

#include "rtp_stream_parser.h"
#include "rtpsession.h"
#include "utils/common/synctime.h"
#include "utils/common/log.h"

static const char H264_NAL_PREFIX[4] = {0x00, 0x00, 0x00, 0x01};
static const char H264_NAL_SHORT_PREFIX[3] = {0x00, 0x00, 0x01};
static const int DEFAULT_SLICE_SIZE = 1024 * 1024;
static const int MAX_ALLOWED_FRAME_SIZE = 1024*1024*10;

CLH264RtpParser::CLH264RtpParser():
        QnRtpVideoStreamParser(),
        m_spsInitialized(false),
        m_frequency(90000), // default value
        m_rtpChannel(98),
        m_prevSequenceNum(-1),
        m_builtinSpsFound(false),
        m_builtinPpsFound(false),
        m_keyDataExists(false),
        m_frameExists(false),
        m_firstSeqNum(0),
        m_packetPerNal(0),
        m_videoFrameSize(0)
        //m_videoBuffer(CL_MEDIA_ALIGNMENT, 1024*128)
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
                        foreach(QByteArray nal, nalUnits)
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

void CLH264RtpParser::serializeSpsPps(QnByteArray& dst)
{
    for (int i = 0; i < m_sdpSpsPps.size(); ++i)
        dst.uncheckedWrite(m_sdpSpsPps[i].data(), m_sdpSpsPps[i].size());
    /*
    if (m_builtinSpsFound && m_builtinPpsFound)
    {
        QMap<int, QByteArray>::const_iterator itr;
        for(itr = m_allNonSliceNal.begin(); itr != m_allNonSliceNal.end(); ++itr)
            dst.write(itr.value());
    }
    else
    {
        for (int i = 0; i < m_sdpSpsPps.size(); ++i)
            dst.write(m_sdpSpsPps[i]);
    }
    */
}

void CLH264RtpParser::decodeSpsInfo(const QByteArray& data)
{
    try
    {
        m_sps.decodeBuffer( (const quint8*) data.constData(), (const quint8*) data.constData() + data.size());
        m_sps.deserialize();
        m_spsInitialized = true;

    } catch(BitStreamException& e)
    {
        // bitstream error
        qWarning() << "Can't deserialize SPS unit. bitstream error" << e.what();
    }
}

QnCompressedVideoDataPtr CLH264RtpParser::createVideoData(const quint8* rtpBuffer, quint32 rtpTime, const RtspStatistic& statistics)
{
    int addheaderSize = 0;
    if (m_keyDataExists && (!m_builtinSpsFound || !m_builtinPpsFound))
        addheaderSize = getSpsPpsSize();

    QnWritableCompressedVideoDataPtr result = QnWritableCompressedVideoDataPtr(new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT, m_videoFrameSize + addheaderSize));
    result->compressionType = CODEC_ID_H264;
    result->width = m_spsInitialized ? m_sps.getWidth() : -1;
    result->height = m_spsInitialized ? m_sps.getHeight() : -1;
    if (m_keyDataExists) {
        result->flags = QnAbstractMediaData::MediaFlags_AVKey;
        if (!m_builtinSpsFound || !m_builtinPpsFound)
            serializeSpsPps(result->m_data);
    }
    //result->data.write(m_videoBuffer);
    size_t spsNaluStartOffset = (size_t)-1;
    size_t spsNaluSize = 0;
    for (uint i = 0; i < m_chunks.size(); ++i)
    {
        if (m_chunks[i].nalStart)
        {
            if( (spsNaluStartOffset != (size_t)-1) && (spsNaluSize == 0) )
                spsNaluSize = result->m_data.size() - spsNaluStartOffset;
            result->m_data.uncheckedWrite(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
            if( (m_chunks[i].len > 0) && ((*((const char*)rtpBuffer + m_chunks[i].bufferOffset) & 0x1f) == nuSPS) )
                spsNaluStartOffset = result->m_data.size();
        }
        result->m_data.uncheckedWrite((const char*) rtpBuffer + m_chunks[i].bufferOffset, m_chunks[i].len);
    }

    if( (spsNaluStartOffset != (size_t)-1) )
        //decoding sps to detect stream resolution change
        decodeSpsInfo( QByteArray::fromRawData( result->m_data.constData() + spsNaluStartOffset, spsNaluSize ) );

    if (m_timeHelper) {
        result->timestamp = m_timeHelper->getUsecTime(rtpTime, statistics, m_frequency);
#ifdef _DEBUG
        qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch()*1000;
        if (qAbs(currentTime - result->timestamp) > 500*1000) {
            //qDebug() << "large RTSP video jitter " << (result->timestamp - currentTime)/1000  << "RtpTime=" << rtpTime;
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
    m_keyDataExists = m_builtinPpsFound = m_builtinSpsFound = false;
    m_frameExists = false;
    m_packetPerNal = 0;
    return false;
}

void CLH264RtpParser::updateNalFlags(int nalUnitType)
{
    if (nalUnitType == nuSPS)
        m_builtinSpsFound = true;
    else if (nalUnitType == nuPPS)
        m_builtinPpsFound = true;
    else if (nalUnitType >= nuSliceNonIDR && nalUnitType <= nuSliceIDR)
    {
        m_frameExists = true;
        if (nalUnitType == nuSliceIDR)
            m_keyDataExists = true;
    }
}

bool CLH264RtpParser::processData(quint8* rtpBufferBase, int bufferOffset, int readed, const RtspStatistic& statistics, bool& gotData)
{
    gotData = false;
    quint8* rtpBuffer = rtpBufferBase + bufferOffset;
    int nalUnitLen;
    //int don;
    quint8 nalUnitType;

    if (readed < RtpHeader::RTP_HEADER_SIZE + 1) 
        return clearInternalBuffer();

    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
    if (rtpHeader->extension)
    {
        if (readed < RtpHeader::RTP_HEADER_SIZE + 4)
            return clearInternalBuffer();

        int extWords = ((int(curPtr[2]) << 8) + curPtr[3]);
        curPtr += extWords*4 + 4;
    }

    const quint8* bufferEnd = rtpBuffer + readed;
    quint16 sequenceNum = ntohs(rtpHeader->sequence);

    if (rtpHeader->payloadType != m_rtpChannel)
        return true; // skip data

    if (curPtr >= bufferEnd)
        return clearInternalBuffer();

    bool isPacketLost = m_prevSequenceNum != -1 && quint16(m_prevSequenceNum) != quint16(sequenceNum-1);
    
    if (m_videoFrameSize > MAX_ALLOWED_FRAME_SIZE)
    {
        NX_LOG("Too large RTP/H.264 frame. Truncate video buffer", cl_logWARNING);
        clearInternalBuffer();
        isPacketLost = true;
    }

    if (isPacketLost) {
        if (m_timeHelper) {
            NX_LOG(QString(lit("RTP Packet loss detected for camera %1. Old seq=%2, new seq=%3"))
                .arg(m_timeHelper->getResID()).arg(m_prevSequenceNum).arg(sequenceNum), cl_logWARNING);
        }
        else {
            NX_LOG("RTP Packet loss detected!!!!", cl_logWARNING);
        }
        clearInternalBuffer();
        emit packetLostDetected(m_prevSequenceNum, sequenceNum);
    }
    m_prevSequenceNum = sequenceNum;

    if (rtpHeader->padding)
        bufferEnd -= bufferEnd[-1];

    m_packetPerNal++;

    int packetType = *curPtr & 0x1f;
    int nalRefIDC = *curPtr++ & 0xe0;

    switch (packetType)
    {
        case STAP_B_PACKET:
            if (bufferEnd-curPtr < 2)
                return clearInternalBuffer();
            //don = (curPtr[0] << 8) + curPtr[1];
            curPtr += 2;
        case STAP_A_PACKET:
            while (curPtr < bufferEnd)
            {
                if (bufferEnd-curPtr < 2)
                    return clearInternalBuffer();
                nalUnitLen = (curPtr[0] << 8) + curPtr[1];
                curPtr += 2;
                if (bufferEnd-curPtr < nalUnitLen)
                    return clearInternalBuffer();

                nalUnitType = *curPtr & 0x1f;
                //m_videoBuffer.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
                //m_videoBuffer.write((const char*)curPtr, nalUnitLen);
                m_chunks.push_back(Chunk(curPtr-rtpBufferBase, nalUnitLen, true));
                m_videoFrameSize += nalUnitLen + 4;
                updateNalFlags(nalUnitType);
                curPtr += nalUnitLen;
            }
            break;
        case FU_B_PACKET:
        case FU_A_PACKET:
            if (bufferEnd-curPtr < 1)
                return clearInternalBuffer();
            nalUnitType = *curPtr & 0x1f;
            updateNalFlags(nalUnitType);
            if (*curPtr  & 0x80) // FU_A first packet
            {
                m_firstSeqNum = sequenceNum;
                m_packetPerNal = 0;
                //m_videoBuffer.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
                nalUnitType |= nalRefIDC;
                //m_videoBuffer.write( (const char*) &nalUnitType, 1);
            }
            else {
                // if packet loss occured in the middle of FU packet, reset flag.
                // packet loss will be reported on the last FU packet. So, do not report problem twice
                isPacketLost = false;
            }

            if (*curPtr  & 0x40) // FU_A last packet
            {
                if (quint16(sequenceNum - m_firstSeqNum) != m_packetPerNal)
                    return clearInternalBuffer(); // packet loss detected
            }

            curPtr++;
            if (packetType == FU_B_PACKET)
            {
                if (bufferEnd-curPtr < 2)
                    return clearInternalBuffer();
                //don = (curPtr[0] << 8) + curPtr[1];
                curPtr += 2;

            }
            //m_videoBuffer.write( (const char*) curPtr, bufferEnd - curPtr);
            if (m_packetPerNal == 0) {// FU_A first packetf
                --curPtr;
                *curPtr = nalUnitType;
            }
            m_chunks.push_back(Chunk(curPtr-rtpBufferBase, bufferEnd - curPtr, m_packetPerNal == 0));
            m_videoFrameSize += bufferEnd - curPtr + (m_packetPerNal == 0 ? 4 : 0);
            break;
        case MTAP16_PACKET:
        case MTAP24_PACKET:
            // not implemented
            return clearInternalBuffer();
        default:
            curPtr--;
            nalUnitType = *curPtr & 0x1f;
            //m_videoBuffer.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
            //m_videoBuffer.write((const char*) curPtr, bufferEnd - curPtr);
            m_chunks.push_back(Chunk(curPtr-rtpBufferBase, bufferEnd - curPtr, true));
            m_videoFrameSize += bufferEnd - curPtr + 4;
            updateNalFlags(nalUnitType);
            break; // ignore unknown data
    }

    if (isPacketLost && !m_keyDataExists)
        return clearInternalBuffer();

    if (rtpHeader->marker && m_frameExists) {
        m_mediaData = createVideoData(rtpBufferBase, ntohl(rtpHeader->timestamp), statistics); // last packet
        gotData = true;
    }
    return true;
}

#endif // ENABLE_DATA_PROVIDERS
