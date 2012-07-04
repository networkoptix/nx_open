#include "h264_rtp_parser.h"
#include "rtp_stream_parser.h"
#include "rtpsession.h"
#include "utils/common/synctime.h"

static const char H264_NAL_PREFIX[4] = {0x00, 0x00, 0x00, 0x01};
static const char H264_NAL_SHORT_PREFIX[3] = {0x00, 0x00, 0x01};
static const int DEFAULT_SLICE_SIZE = 1024 * 1024;

CLH264RtpParser::CLH264RtpParser():
        QnRtpStreamParser(),
        m_frequency(90000), // default value
        m_rtpChannel(98),
        m_builtinSpsFound(false),
        m_builtinPpsFound(false),
        m_keyDataExists(false),
        m_timeCycles(0),
        m_lastTimeStamp(0),
        m_firstSeqNum(0),
        m_packetPerNal(0),
        m_videoBuffer(CL_MEDIA_ALIGNMENT, 1024*128),
        m_prevSequenceNum(-1)
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
            if (!codecName.startsWith("H264"))
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
        else if (lines[i].startsWith("a=fmtp:"))
        {
            //QList<QByteArray> values = lines[i].split(' ');
            int valueIndex = lines[i].indexOf(' ');
            if (valueIndex == -1)
                continue;

            QList<QByteArray> fmpParam = lines[i].left(valueIndex).split(':'); //values[0].split(':');
            if (fmpParam.size() < 2 || fmpParam[1].toUInt() != m_rtpChannel)
                continue;
            //if (values.size() < 2)
            //    continue;
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

void CLH264RtpParser::serializeSpsPps(CLByteArray& dst)
{
    for (int i = 0; i < m_sdpSpsPps.size(); ++i)
        dst.write(m_sdpSpsPps[i]);
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
        m_sps.decodeBuffer( (const quint8*) data.data() + sizeof(H264_NAL_PREFIX), (const quint8*) data.data() + data.size());
        m_sps.deserialize();

    } catch(BitStreamException& e)
    {
        // bitstream error
        qWarning() << "Can't deserialize SPS unit. bitstream error" << e.what();
    }
}

QnCompressedVideoDataPtr CLH264RtpParser::createVideoData(quint32 rtpTime, const RtspStatistic& statistics)
{
    int addheaderSize = 0;
    if (m_keyDataExists && (!m_builtinSpsFound || !m_builtinPpsFound))
        addheaderSize = getSpsPpsSize();
    QnCompressedVideoDataPtr result = QnCompressedVideoDataPtr(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, m_videoBuffer.size() + addheaderSize));
    result->compressionType = CODEC_ID_H264;
    result->width = m_sps.getWidth();
    result->height = m_sps.getHeight();
    if (m_keyDataExists) {
        result->flags = AV_PKT_FLAG_KEY;
        if (!m_builtinSpsFound || !m_builtinPpsFound)
            serializeSpsPps(result->data);
    }
    result->data.write(m_videoBuffer);

    if (m_timeHelper) {
        result->timestamp = m_timeHelper->getUsecTime(rtpTime, statistics, m_frequency);
        //qDebug() << "video. adjusttime to " << (m_videoData->timestamp - qnSyncTime->currentMSecsSinceEpoch()*1000)/1000 << "ms";
    }
    else
        result->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;

    clearInternalBuffer();
    return result;
}

bool CLH264RtpParser::clearInternalBuffer()
{
    m_videoBuffer.clear();
    m_keyDataExists = m_builtinPpsFound = m_builtinSpsFound = false;
    m_packetPerNal = INT_MIN;
    return false;
}

void CLH264RtpParser::updateNalFlags(int nalUnitType)
{
    m_builtinSpsFound |= nalUnitType == nuSPS;
    m_builtinPpsFound |= nalUnitType == nuPPS;
    m_keyDataExists   |= nalUnitType == nuSliceIDR;
}

bool CLH264RtpParser::processData(quint8* rtpBuffer, int readed, const RtspStatistic& statistics, QList<QnAbstractMediaDataPtr>& result)
{
    result.clear();

    int nalUnitLen;
    int don;
    quint8 nalUnitType;

    if (readed < RtpHeader::RTP_HEADER_SIZE + 1) 
        return clearInternalBuffer();
    
    RtpHeader* rtpHeader = (RtpHeader*) rtpBuffer;
    quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
    quint8* bufferEnd = rtpBuffer + readed;
    quint16 sequenceNum = ntohs(rtpHeader->sequence);

    

    if (m_prevSequenceNum != -1 && quint16(m_prevSequenceNum) != quint16(sequenceNum-1)) {
        m_prevSequenceNum = sequenceNum;
        qWarning() << "RTP Packet lost detected!";
        return clearInternalBuffer();
    }
    m_prevSequenceNum = sequenceNum;

    if (rtpHeader->padding)
        --bufferEnd;

    int packetType = *curPtr++ & 0x1f;

    m_packetPerNal++;

    switch (packetType)
    {
        case STAP_B_PACKET:
            if (bufferEnd-curPtr < 2)
                return clearInternalBuffer();
            don = (*curPtr++ << 8) + *curPtr++;
        case STAP_A_PACKET:
            if (bufferEnd-curPtr < 2)
                return clearInternalBuffer();
            nalUnitLen = (*curPtr++ << 8) + *curPtr++;
            if (bufferEnd-curPtr < nalUnitLen)
                return clearInternalBuffer();

            nalUnitType = *curPtr & 0x1f;
            m_videoBuffer.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
            m_videoBuffer.write((const char*)curPtr, nalUnitLen);
            updateNalFlags(nalUnitType);
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
                m_videoBuffer.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
                nalUnitType += 0x40;
                m_videoBuffer.write( (const char*) &nalUnitType, 1);
            }
            if (*curPtr  & 0x40) // FU_A last packet
            {
                if (quint16(sequenceNum - m_firstSeqNum) != m_packetPerNal)
                {
                    clearInternalBuffer();
                    return true; // packet lost detected. wait more data (return no error because of packet lost already reported)
                }
            }
            curPtr++;
            if (packetType == FU_B_PACKET)
            {
                if (bufferEnd-curPtr < 2)
                    return clearInternalBuffer();
                don = (*curPtr++ << 8) + *curPtr++;
            }
            m_videoBuffer.write( (const char*) curPtr, bufferEnd - curPtr);
            break;
        case MTAP16_PACKET:
        case MTAP24_PACKET:
            // not implemented
            return clearInternalBuffer();
        default:
            curPtr--;
            nalUnitType = *curPtr & 0x1f;
            m_videoBuffer.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
            m_videoBuffer.write((const char*) curPtr, bufferEnd - curPtr);
            updateNalFlags(nalUnitType);
            break; // ignore unknown data
        }

    if (rtpHeader->marker) 
    {   // last packet
        if (m_videoBuffer.size() > 0)
            result << createVideoData(ntohl(rtpHeader->timestamp), statistics);
    }

    return true;
}
