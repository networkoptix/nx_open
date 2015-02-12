#include "utils/network/socket.h"
#include "rtsp_h264_encoder.h"
#include "utils/media/nalUnits.h"
#include "utils/network/rtp_stream_parser.h"

QnRtspH264Encoder::QnRtspH264Encoder():
    m_currentData(0),
    m_nalEnd(0),
    m_nalStarted(false),
    m_isFragmented(false)
{

}

QByteArray QnRtspH264Encoder::getAdditionSDP()
{
    if (!m_sdpMediaPacket) 
        return QByteArray();
    
    const quint8* end = (const quint8*) m_sdpMediaPacket->data() + m_sdpMediaPacket->dataSize();
    const quint8* curPtr = NALUnit::findNextNAL((const quint8*) m_sdpMediaPacket->data(), end);
    const quint8* nalEnd = NALUnit::findNALWithStartCode(curPtr, end, true);
    bool firstStep = true;
    QString propParam;
    QString profileParam;
    while (curPtr < end) 
    {
        quint8 nalType = *curPtr & 0x1f;
        if (nalType == nuSPS || nalType == nuPPS)
        {
            if (nalType == nuSPS && end - curPtr >= 4)
            {
                profileParam = QByteArray((const char*)curPtr+1, 3).toHex();
            }

            if (!firstStep)
                propParam += ',';
            propParam += QByteArray((const char*) curPtr, nalEnd - curPtr).toBase64();
            firstStep = false;
        }
        curPtr = NALUnit::findNextNAL(nalEnd, end);
        nalEnd = NALUnit::findNALWithStartCode(curPtr, end, true);
    }

    QString result("a=fmtp:%1 packetization-mode=1; profile-level-id=%2; sprop-parameter-sets=%3\r\n");
    return result.arg(getPayloadtype()).arg(profileParam).arg(propParam).toLatin1();
}

void QnRtspH264Encoder::goNextNal()
{
    const quint8* dataEnd = (const quint8*) m_media->data() + m_media->dataSize();
    m_currentData = NALUnit::findNextNAL(m_nalEnd, dataEnd);
    m_nalEnd = NALUnit::findNALWithStartCode(m_currentData, dataEnd, true);
    m_isFragmented = m_nalEnd - m_currentData > RTSP_H264_MAX_LEN;
    m_nalStarted = true;
    if (m_currentData < m_nalEnd) {
        m_nalType = *m_currentData & 0x1f;
        m_nalRefIDC = *m_currentData & 0x60;

        //if (m_nalType <= nuSliceIDR)
        //    m_isFragmented = true;

        if (m_isFragmented)
            m_currentData++; // ignore nal code from source data
    }
}

void QnRtspH264Encoder::setDataPacket(QnConstAbstractMediaDataPtr media)
{
    if (media->dataType != QnAbstractMediaData::VIDEO)
    {
        m_media.clear();
        return;
    }

    m_media = media;

    m_nalEnd = (const quint8*) m_media->data();
    goNextNal();    
}

bool QnRtspH264Encoder::getNextPacket(QnByteArray& sendBuffer)
{
    if (!m_media)
        return false;

    const quint8* dataEnd = (const quint8*) m_media->data() + m_media->dataSize();
    if (m_currentData == dataEnd)
        return false;

    sendBuffer.resize(sendBuffer.size() + RtpHeader::RTP_HEADER_SIZE); // reserve space for RTP header

    if (!m_isFragmented)
    {
        sendBuffer.write((const char*) m_currentData, m_nalEnd - m_currentData);
        goNextNal();
        return true;
    }
    else 
    {
        // fragmented FU_A_PACKET
        quint8 nalType = m_nalType;

        int payloadLen = qMin((int) (m_nalEnd - m_currentData), RTSP_H264_MAX_LEN - 2);

        // write 2 bytes header
        quint8 rtspNalType = FU_A_PACKET | 0x80 | m_nalRefIDC;
        sendBuffer.write((const char*) &rtspNalType, 1);
        if (m_nalStarted)
        {
            nalType |= 0x80; // set start bit
            m_nalStarted = false;
        }
        if (isLastPacket())
            nalType |= 0x40; // set end bit
        sendBuffer.write((const char*) &nalType, 1);
        
        // write nal payload
        sendBuffer.write((const char*) m_currentData, payloadLen);
        if (isLastPacket()) {
            Q_ASSERT(m_currentData + payloadLen == m_nalEnd);
            goNextNal();
        }
        else {
            m_currentData += payloadLen;
            Q_ASSERT(m_currentData < m_nalEnd);
        }
    }
    return true;
}

void QnRtspH264Encoder::init()
{

}

quint32 QnRtspH264Encoder::getSSRC()
{
    return RTSP_H264_PAYLOAD_SSRC;
}

bool QnRtspH264Encoder::isLastPacket()
{
    return m_isFragmented || m_currentData == m_nalEnd;
}

bool QnRtspH264Encoder::getRtpMarker()
{
    if (!m_media)
        return false;

    // return true if last packet
    const quint8* dataEnd = (const quint8*) m_media->data() + m_media->dataSize();
    return m_nalEnd == dataEnd && isLastPacket();
}

quint32 QnRtspH264Encoder::getFrequency()
{
    return RTSP_H264_PAYLOAD_FREQ;
}

quint8 QnRtspH264Encoder::getPayloadtype()
{
    return RTSP_H264_PAYLOAD_TYPE;
}

QString QnRtspH264Encoder::getName()
{
    return QLatin1String("H264");
}
