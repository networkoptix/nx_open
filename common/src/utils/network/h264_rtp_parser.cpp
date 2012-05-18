#include "h264_rtp_parser.h"
#include "rtp_stream_parser.h"
#include "rtpsession.h"
#include "utils/common/synctime.h"

static const int MAX_RTP_PACKET_SIZE = 1024 * 8;
static const char H264_NAL_PREFIX[4] = {0x00, 0x00, 0x00, 0x01};
static const char H264_NAL_SHORT_PREFIX[3] = {0x00, 0x00, 0x01};
static const int DEFAULT_SLICE_SIZE = 1024 * 1024;

CLH264RtpParser::CLH264RtpParser(RTPIODevice* input):
        CLRtpStreamParser(input),
        m_frequency(90000), // default value
        m_rtpChannel(98),
        m_builtinSpsFound(false),
        m_builtinPpsFound(false),
        m_timeCycles(0),
        m_lastTimeStamp(0),
        m_firstSeqNum(0),
        m_packetPerNal(0)
{
}

CLH264RtpParser::~CLH264RtpParser()
{
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

void CLH264RtpParser::serializeSpsPps(CLByteArray& dst)
{

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

QnAbstractMediaDataPtr CLH264RtpParser::getNextData()
{
//    int lasttSeqNum;
    int nalUnitLen;
    int don;
    quint8 nalUnitType;
    bool firstPacketReceived;
//    int nal_ref_idc = 0;
    RtpHeader* rtpHeader = 0;
    bool isKeyFrame = false;
    int readed = 0;
    quint16 lastSeqNum = 0;

    bool lastPacketReceived = false;
    QnCompressedVideoDataPtr videoData(new QnCompressedVideoData(CL_MEDIA_ALIGNMENT, DEFAULT_SLICE_SIZE));

    while(!lastPacketReceived) // todo: add termination code here
    {
        quint8 rtpBuffer[MAX_RTP_PACKET_SIZE];
        // ioDevice read function MUST return one RTP packet per once

        readed = m_input->read( (char*) rtpBuffer, sizeof(rtpBuffer));

        if (readed < 1)
        {
            return QnAbstractMediaDataPtr();
        }
        if (readed < RtpHeader::RTP_HEADER_SIZE + 1)
        {
            return QnAbstractMediaDataPtr();
        }
        rtpHeader = (RtpHeader*) rtpBuffer;
        //bool isLastPacket = rtpHeader->marker;
        quint8* curPtr = rtpBuffer + RtpHeader::RTP_HEADER_SIZE;
        int bytesLeft = readed - RtpHeader::RTP_HEADER_SIZE;

        if (rtpHeader->padding)
        {
            bytesLeft -= ntohl(rtpHeader->padding);
        }

        int packetType = *curPtr & 0x1f;
        //quint8 stapHeader = *curPtr;
        //bool fBit = *curPtr >> 7;
        //nal_ref_idc = (*curPtr >> 5) & 0x3;
        //isKeyFrame = nal_ref_idc == 0;
        curPtr++;
        bytesLeft--;

        while (bytesLeft > 0)
        {
            switch (packetType)
            {
                case STAP_B_PACKET:
                    if (bytesLeft < 2)
                    {
                        return QnAbstractMediaDataPtr();
                    }
                    don = (*curPtr << 8) + curPtr[1];
                    curPtr += 2;
                    bytesLeft -= 2;
                case STAP_A_PACKET:
                    if (bytesLeft < 2)
                    {
                        return QnAbstractMediaDataPtr();
                    }
                    nalUnitLen = (*curPtr << 8) + curPtr[1];
                    curPtr += 2;
                    bytesLeft -= 2;
                    if (bytesLeft < nalUnitLen)
                    {
                        return QnAbstractMediaDataPtr();
                    }
                    nalUnitType = *curPtr & 0x1f;
                    if ((nalUnitType & 0x1f) >= nuSliceNonIDR && nalUnitType <= nuSliceIDR)
                    {
                        isKeyFrame = (nalUnitType & 0x1f) == nuSliceIDR;
                        // it is slice
                        if (isKeyFrame)
                            serializeSpsPps(videoData->data);
                        videoData->data.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
                        videoData->data.write((const char*)curPtr, nalUnitLen);
                        lastPacketReceived = true;
                    }
                    else
                    {
                        QByteArray& data = m_allNonSliceNal[nalUnitType];
                        data.clear();
                        data += QByteArray(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
                        data += QByteArray( (const char*) curPtr, nalUnitLen);
                        m_builtinSpsFound |= nalUnitType == nuSPS;
                        m_builtinPpsFound |= nalUnitType == nuPPS;
                        if (nalUnitType == nuSPS)
                            decodeSpsInfo(data);
                    }
                    curPtr += nalUnitLen;
                    bytesLeft -= nalUnitLen;
                    break;
                case FU_B_PACKET:
                case FU_A_PACKET:
                    if (bytesLeft < 1)
                    {
                        return QnAbstractMediaDataPtr();
                    }
                    firstPacketReceived = *curPtr  & 0x80;
                    lastPacketReceived = *curPtr & 0x40;

                    if (firstPacketReceived)
                    {
                        videoData->data.clear();
                        m_firstSeqNum = ntohs(rtpHeader->sequence);
                        m_packetPerNal = 0;
                    }
                    else
                        m_packetPerNal++;
                    if (lastPacketReceived)
                    {
                        lastSeqNum = ntohs(rtpHeader->sequence);
                        //if (getSequenceDiff(lastSeqNum, m_firstSeqNum) != m_packetPerNal)
                        if ((quint16) (lastSeqNum - m_firstSeqNum) != m_packetPerNal)
                        {
                            // packet lost detected
                            cl_log.log(QLatin1String("RTP Packet lost detected!!"), cl_logWARNING);
                            //videoData->data.clear();
                            //lastPacketReceived = false;
                            //break;
                            return QnAbstractMediaDataPtr();
                        }

                    }

                    nalUnitType = *curPtr & 0x1f;
                    curPtr++;
                    bytesLeft--;
                    if (packetType == FU_B_PACKET)
                    {
                        if (bytesLeft < 2)
                        {
                            return QnAbstractMediaDataPtr();
                        }
                        don = (*curPtr << 8) + curPtr[1];
                        curPtr += 2;
                        bytesLeft -= 2;
                    }

                    if (nalUnitType >= nuSliceNonIDR && nalUnitType  <= nuSliceIDR)
                    {
                        // it is slice
                        //if (firstPacketReceived)
                        isKeyFrame = nalUnitType == nuSliceIDR;
                        if (videoData->data.size() == 0)
                        {
                            if (isKeyFrame)
                                serializeSpsPps(videoData->data);
                            videoData->data.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
                            nalUnitType += 0x40;
                            videoData->data.write( (const char*) &nalUnitType, 1);
                        }
                        videoData->data.write( (const char*) curPtr, bytesLeft);
                    }
                    else
                    {
                        QByteArray& data = m_allNonSliceNal[nalUnitType];
                        if (firstPacketReceived)
                        {
                            data.clear();
                            data += QByteArray(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
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
                case MTAP16_PACKET:
                case MTAP24_PACKET:
                    // not implemented
                    //return QnAbstractMediaDataPtr();
                    break;
                default:
                    isKeyFrame = (packetType & 0x1f) == nuSliceIDR;
                    if (isKeyFrame)
                        serializeSpsPps(videoData->data);

                    videoData->data.write(H264_NAL_PREFIX, sizeof(H264_NAL_PREFIX));
                    videoData->data.write((const char*) curPtr-1, bytesLeft+1);

                    bytesLeft = 0;
                    lastPacketReceived = true;
                    break; // ignore unknown data
            }
        }
    }
    videoData->width = m_sps.getWidth();
    videoData->height = m_sps.getHeight();


    videoData->channelNumber = 0;
    if (isKeyFrame)
        videoData->flags |= AV_PKT_FLAG_KEY;
    else
        videoData->flags &= ~AV_PKT_FLAG_KEY;
    videoData->compressionType = CODEC_ID_H264;
    if (rtpHeader)
    {
        /*
        quint32 timeStamp = ntohl(rtpHeader->timestamp);
        if (timeStamp < m_lastTimeStamp && timeStamp < 0x40000000u && m_lastTimeStamp > 0xc0000000)
            m_timeCycles += 0x100000000ull;
        quint64 fullTimestamp = (quint64) timeStamp + m_timeCycles;
        videoData->timestamp = fullTimestamp * 1000000ull / m_frequency;
        m_lastTimeStamp = timeStamp;
        */

        //videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;
        videoData->timestamp = qnSyncTime->currentMSecsSinceEpoch() * 1000;
    }

    return videoData;
}

