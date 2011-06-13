#include "h264_rtp_parser.h"

static const int MAX_RTP_PACKET_SIZE = 1024 * 8;
static const char H264_NAL_PREFIX[4] = {0x00, 0x00, 0x00, 0x01};
static const int DEFAULT_SLICE_SIZE = 1024 * 64;

CLH264RtpParser::CLH264RtpParser(QIODevice* input):
        CLRtpStreamParser(input),
        m_frequency(90000), // default value
        m_rtpChannel(98),
        m_builtinSpsFound(false),
        m_builtinPpsFound(false),
        m_timeCycles(0),
        m_lastTimeStamp(0)
{
    m_videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT, DEFAULT_SLICE_SIZE);
}

CLH264RtpParser::~CLH264RtpParser()
{
    delete m_videoData;
}

void CLH264RtpParser::setSDPInfo(const QByteArray& data)
{
    CLRtpStreamParser::setSDPInfo(data);
    QList<QByteArray> lines = data.split('\n');
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
            QList<QByteArray> values = lines[i].split(' ');
            QList<QByteArray> fmpParam = values[0].split(':');
            if (fmpParam.size() < 2 || fmpParam[1].toUInt() != m_rtpChannel)
                continue;
            if (values.size() < 2)
                continue;
            QList<QByteArray> h264Params = values[1].split(';');
            for (int i = 0; i < h264Params.size(); ++i)
            {
                if (h264Params[i] == "sprop-parameter-sets")
                {
                    int pos = h264Params[i].indexOf('=');
                    if (pos >= 0) 
                    {
                        QByteArray h264SpsPps = h264Params[i].mid(pos+1);
                        m_sdpSpsPps = QByteArray::fromBase64(h264SpsPps);
                    }
                }
            }
        }
    }
}

void CLH264RtpParser::serializeSpsPps(CLByteArray& dst)
{
    if (m_builtinSpsFound && m_builtinPpsFound)
    {
        QMap<int, QByteArray>::const_iterator itr;
        for(itr = m_allNonSliceNal.begin(); itr != m_allNonSliceNal.end(); ++itr)
            dst.write(itr.value());
    }
    else {
        dst.write(m_sdpSpsPps);
    }
}

void CLH264RtpParser::decodeSpsInfo(const QByteArray& data)
{
    try {
        m_sps.decodeBuffer( (const quint8*) data.data(), (const quint8*) data.data() + data.size());
        m_sps.deserialize();
    } catch(BitStreamException& e) {
        // bitstream error
        qWarning() << "Can't deserialize SPS unit. bitstream error" << e.what();
    }
}

CLAbstractMediaData* CLH264RtpParser::getNextData()
{
    int nalUnitLen;
    int don;
    quint8 nalUnitType;
    bool firstPacketReceived;
    int nal_ref_idc = 0;
    RtpHeader* rtpHeader = 0;

    bool lastPacketReceived = false;

    while(!lastPacketReceived) // todo: add termination code here
    {
        quint8 rtpBuffer[MAX_RTP_PACKET_SIZE];
        // ioDevice read function MUST return one RTP packet per once
        int readed = m_input->read( (char*) rtpBuffer, sizeof(rtpBuffer));
        if (readed -1)
            return 0;
        if (readed < sizeof(RtpHeader) + 1)
            return 0; 
        rtpHeader = (RtpHeader*) rtpBuffer;
        //bool isLastPacket = rtpHeader->marker;
        quint8* curPtr = rtpBuffer + sizeof(rtpHeader);
        int bytesLeft = readed - sizeof(rtpHeader);

        int packetType = *curPtr & 0x1f; 
        //bool fBit = *curPtr >> 7;
        nal_ref_idc = (*curPtr >> 5) & 0x3;
        bool isKeyFrame = nal_ref_idc != 0;
        curPtr++;
        bytesLeft--;

        while (bytesLeft > 0)
        {
            switch (packetType)
            {
                case STAP_B_PACKET:
                    if (bytesLeft < 2)
                        return 0; // too few data
                    don = (*curPtr << 8) + curPtr[1];
                    curPtr += 2;
                    bytesLeft -= 2;
                case STAP_A_PACKET:
                    if (bytesLeft < 2)
                        return 0; // too few data
                    nalUnitLen = (*curPtr << 8) + curPtr[1];
                    curPtr += 2;
                    bytesLeft -= 2;
                    if (bytesLeft < nalUnitLen)
                        return 0; // too few data
                    nalUnitType = *curPtr & 0x1f;
                    if ((nalUnitType & 0x1f) >= nuSliceNonIDR && nalUnitType <= nuSliceIDR)
                    {
                        // it is slice
                        if (isKeyFrame)
                            serializeSpsPps(m_videoData->data);
                        m_videoData->data.write(H264_NAL_PREFIX);
                        m_videoData->data.write((const char*)curPtr, nalUnitLen);
                    }
                    else 
                    {
                        QByteArray& data = m_allNonSliceNal[nalUnitType];
                        data.clear();
                        data += H264_NAL_PREFIX;
                        data += QByteArray( (const char*) curPtr, nalUnitLen);
                        m_builtinSpsFound |= nalUnitType == nuSPS;
                        m_builtinPpsFound |= nalUnitType == nuPPS;
                        if (nalUnitType == nuSPS)
                            decodeSpsInfo(data);
                    }
                    curPtr += nalUnitLen;
                    bytesLeft -= nalUnitLen;
                    lastPacketReceived = true;
                    break;
                case FU_B_PACKET:
                case FU_A_PACKET:
                    if (bytesLeft < 1)
                        return 0; // too few data
                    firstPacketReceived = *curPtr & 0x80;
                    lastPacketReceived = *curPtr & 0x40;
                    curPtr++;
                    bytesLeft--;
                    if (bytesLeft < 1)
                        return 0; // not enought data

                    nalUnitType = *curPtr;
                    curPtr++;
                    bytesLeft--;
                    if (packetType == FU_B_PACKET)
                    {
                        if (bytesLeft < 2)
                            return 0; // too few data
                        don = (*curPtr << 8) + curPtr[1];
                        curPtr += 2;
                        bytesLeft -= 2;
                    }

                    if ((nalUnitType & 0x1f) >= nuSliceNonIDR && nalUnitType <= nuSliceIDR)
                    {
                        // it is slice
                        if (firstPacketReceived) 
                        {
                            if (isKeyFrame)
                                serializeSpsPps(m_videoData->data);
                            m_videoData->data.write(H264_NAL_PREFIX);
                            m_videoData->data.write( (const char*) &nalUnitType, 1);
                        }
                        m_videoData->data.write( (const char*) curPtr, bytesLeft);
                    }
                    else 
                    {
                        QByteArray& data = m_allNonSliceNal[nalUnitType];
                        if (firstPacketReceived) 
                        {
                            data.clear();
                            data += H264_NAL_PREFIX;
                            data += nalUnitType;
                        }
                        data += QByteArray( (const char*) curPtr, bytesLeft);
                        m_builtinSpsFound |= nalUnitType == nuSPS;
                        m_builtinPpsFound |= nalUnitType == nuPPS;
                        if (nalUnitType == nuSPS)
                            decodeSpsInfo(data);
                    }
                    bytesLeft = 0;
                    break;
                MTAP16_PACKET:
                    return 0; // todo not implemented yet
                MTAP24_PACKET:
                    return 0; // todo not implemented yet
                default:
                    return 0; // invalid packet type
            }
        }
    }
    m_videoData->width = m_sps.getWidth();
    m_videoData->height = m_sps.getHeight();
    m_videoData->channelNumber = 0;
    m_videoData->keyFrame = nal_ref_idc != 0;
    m_videoData->compressionType = CODEC_ID_H264;
    if (rtpHeader) 
    {
        if (rtpHeader->timestamp < m_lastTimeStamp && rtpHeader->timestamp < 0x40000000u && m_lastTimeStamp > 0xc0000000)
            m_timeCycles += 0x100000000ull;
        quint64 fullTimestamp = (quint64) rtpHeader->timestamp + m_timeCycles; 
        m_videoData->timestamp = fullTimestamp * 1000000ull / m_frequency;
        m_lastTimeStamp = rtpHeader->timestamp;
    }
    CLCompressedVideoData* result = m_videoData;
    m_videoData = new CLCompressedVideoData(CL_MEDIA_ALIGNMENT, DEFAULT_SLICE_SIZE);
    return result;
}
