#include "rtpsession.h"
#include "rtp_stream_parser.h"


#define DEFAULT_RTP_PORT 554
#define RESERVED_TIMEOUT_TIME (5*1000)

static const int MAX_RTCP_PACKET_SIZE = 1024 * 2;
static const quint32 SSRC_CONST = 0x2a55a9e8;
static const quint32 CSRC_CONST = 0xe8a9552a;


RTPIODevice::RTPIODevice(RTPSession& owner):
m_owner(owner),
m_receivedPackets(0),
m_receivedOctets(0),
m_tcpMode(false)
{

}

RTPIODevice::~RTPIODevice()
{

}

qint64	RTPIODevice::read( char * data, qint64 maxSize )
{

    int readed;
    if (m_tcpMode)
        readed = m_owner.readBinaryResponce((quint8*) data, maxSize); // demux binary data from TCP socket
    else 
        readed = m_sock->recv(data, maxSize);
    if (readed > 0)
    {
        m_receivedPackets++;
        m_receivedPackets += readed;
    }
    RtspStatistic stats;
    stats.nptTime = 0;
    stats.timestamp = 0;
    stats.receivedOctets = m_receivedPackets;
    stats.receivedPackets = m_receivedPackets;
    m_owner.sendKeepAliveIfNeeded(&stats);
    return readed;
}


//============================================================================================
RTPSession::RTPSession():
    m_csec(2),
    m_udpSock(0),
    m_rtcpUdpSock(0),
    m_rtpIo(*this),
    m_transport("UDP")
{
    m_udpSock.setTimeOut(500);
    m_tcpSock.setTimeOut(3 * 1000);
    m_responseBuffer = new quint8[MAX_RESPONCE_LEN];
    m_responseBufferLen = 0;
}

RTPSession::~RTPSession()
{
    delete [] m_responseBuffer;
}

void RTPSession::parseSDP()
{
    QList<QByteArray> lines = m_sdp.split('\n');

    int mapNum = -1; 
    QString codecName;
    foreach(QByteArray line, lines)
    {
        line = line.trimmed().toLower();
        if (line.startsWith("a=rtpmap"))
        {
            QList<QByteArray> params = line.split(' ');
            if (params.size() < 2)
                continue; // invalid data format. skip
            QList<QByteArray> trackInfo = params[0].split(':');
            QList<QByteArray> codecInfo = params[1].split('/');
            if (trackInfo.size() < 2 || codecInfo.size() < 2)
                continue; // invalid data format. skip
            mapNum = trackInfo[1].toUInt();
            codecName = codecInfo[0];
        }
        else if (line.startsWith("a=control:trackid="))
        {
            int trackNum = line.mid(QString("a=control:trackid=").length()).toUInt();
            m_sdpTracks.insert(trackNum, codecName);
        }
    }
}


bool RTPSession::open(const QString& url)
{
    mUrl = url; 

    unsigned int port = DEFAULT_RTP_PORT;

    if (!m_tcpSock.connect(mUrl.host().toLatin1().data(), DEFAULT_RTP_PORT))
        return false;

    if (!sendDescribe())
        return false;

    QByteArray sdp;

    if (!readTextResponce(sdp))
        return false;

    int sdp_index = sdp.indexOf("\r\n\r\n");

    if (sdp_index  < 0 || sdp_index+4 >= sdp.size())
        return false;

    m_sdp = sdp.mid(sdp_index+4);
    parseSDP();

    if (m_sdpTracks.size()<=0)
        return false;

    return true;
}

RTPIODevice* RTPSession::play()
{
    RTPIODevice* ioDevice = sendSetup();

    if (ioDevice == 0)
        return 0;

    
    if (!sendPlay())
        return 0;


    return &m_rtpIo;

}

bool RTPSession::stop()
{

    return true;
}


bool RTPSession::isOpened() const
{
    return m_tcpSock.isConnected();
}

unsigned int RTPSession::sessionTimeoutMs()
{
    return 0;
}

const QByteArray& RTPSession::getSdp() const
{
    return m_sdp;
}

//===================================================================

bool RTPSession::sendDescribe()
{
    QByteArray request;
    request += "DESCRIBE ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    request += "User-Agent: Network Optix\r\n";
    request += "Accept: application/sdp\r\n\r\n";
    
    //qDebug() << request;

    if (!m_tcpSock.send(request.data(), request.size()))
        false;

    return true;

}

bool RTPSession::sendOptions()
{
    QByteArray request;
    request += "OPTIONS ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n\r\n";


    if (!m_tcpSock.send(request.data(), request.size()))
        false;

    return true;

}

RTPIODevice*  RTPSession::sendSetup()
{
    if (m_transport == "UDP")
        m_rtpIo.setSocket(&m_udpSock);
    else
        m_rtpIo.setSocket(&m_tcpSock);
    for (QMap<int, QString>::iterator itr = m_sdpTracks.begin(); itr != m_sdpTracks.end(); ++itr)
    {
        QByteArray request;
        request += "SETUP ";
        request += mUrl.toString();

        if (!m_sdpTracks.isEmpty())
            request += QString("/trackID=") + QString::number(itr.key());

        request += " RTSP/1.0\r\n";
        request += "CSeq: ";
        request += QByteArray::number(m_csec++);
        request += "\r\n";
        request += "User-Agent: Network Optix\r\n";
        request += QString("Transport: RTP/AVP/") + m_transport + QString(";unicast");
        
        if (m_transport == "UDP")
        {
            request += ";client_port=";
            request += QString::number(m_udpSock.getLocalPort());
            request += ',';
            request += QString::number(m_rtcpUdpSock.getLocalPort());
        }
        else 
        {
            request += "interleaved=" + QString::number(itr.key()*2) + QString("-") + QString::number(itr.key()*2+1);
            // todo: send TCP transport info here
        }
        request += "\r\n\r\n";

        //qDebug() << request;

        if (!m_tcpSock.send(request.data(), request.size()))
            return 0;

        QByteArray responce;

        if (!readTextResponce(responce))
            return 0;


        if (!responce.startsWith("RTSP/1.0 200")) 
        {
            return 0;
        }
        m_TimeOut = 0; // default timeout 0 ( do not send keep alive )

        QString tmp = extractRTSPParam(responce, "Session:");

        if (tmp.size() > 0) 
        {
            QStringList tmpList = tmp.split(';');
            m_SessionId = tmpList[0];

            for (int i = 0; i < tmpList.size(); ++i)
            {
                if (tmpList[i].startsWith("timeout"))
                {
                    QStringList tmpParams = tmpList[i].split('=');
                    if (tmpParams.size() > 1)
                        m_TimeOut = tmpParams[1].toInt();
                }
            }
        }

        updateTransportHeader(responce);
    }

    return &m_rtpIo;
}

bool RTPSession::sendPlay()
{

    QByteArray request;
    QByteArray responce;

    request += "PLAY ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    request += "Session: ";
    request += m_SessionId;
    request += "\r\n";
    request += "Range: npt=0.000"; // offset
    request += "-\r\n\r\n";

    if (!m_tcpSock.send(request.data(), request.size()))
        return false;


    if (!readTextResponce(responce))
        return false;


    if (responce.startsWith("RTSP/1.0 200"))
    {
        updateTransportHeader(responce);
        m_keepAliveTime.restart();
        return true;
    }

    return false;

}

bool RTPSession::sendTeardown()
{
    QByteArray request;
    QByteArray responce;
    request += "TEARDOWN ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    request += "Session: ";
    request += m_SessionId;
    request += "\r\n\r\n";

    if (!m_tcpSock.send(request.data(), request.size()))
        false;


    
    if (!readTextResponce(responce) || !responce.startsWith("RTSP/1.0 200"))
    {
        return false;
    }
    else 
    {
        //d->lastSendTime.start();
        return 0;
    }
}


static const int RTCP_RECEIVER_REPORT = 201;
static const int RTCP_SOURCE_DESCRIPTION = 202;

int RTPSession::buildRTCPReport(quint8* dstBuffer, const RtspStatistic* stats)
{
    QByteArray esDescr("netoptix");


    quint8* curBuffer = dstBuffer;
    *curBuffer++ = (RtpHeader::RTP_VERSION << 6); 
    *curBuffer++ = RTCP_RECEIVER_REPORT;  // packet type
    curBuffer += 2; // skip len field;

    quint32* curBuf32 = (quint32*) curBuffer;
    *curBuf32++ = htonl(SSRC_CONST);
    *curBuf32++ = htonl((quint32) stats->nptTime);
    quint32 fracTime = (quint32) ((stats->nptTime - (quint32) stats->nptTime) * UINT_MAX); // UINT32_MAX
    *curBuf32++ = htonl(fracTime);
    *curBuf32++ = htonl(stats->timestamp);
    *curBuf32++ = htonl(stats->receivedPackets);
    *curBuf32++ = htonl(stats->receivedOctets);

    // correct len field (count of 32 bit words -1)
    quint16 len = (quint16) (curBuf32 - (quint32*) dstBuffer);
    * ((quint16*) (dstBuffer + 2)) = htons(len-1);
    
    // build source description
    curBuffer = (quint8*) curBuf32;
    *curBuffer++ = (RtpHeader::RTP_VERSION << 6) + 1;  // source count = 1
    *curBuffer++ = RTCP_SOURCE_DESCRIPTION;  // packet type
    *curBuffer++ = 0; // len field = 6 (hi)
    *curBuffer++ = 6; // len field = 6 (low)
    curBuf32 = (quint32*) curBuffer;
    *curBuf32 = htonl(CSRC_CONST);
    curBuffer+=4;
    *curBuffer++ = 1; // ES_TYPE CNAME
    *curBuffer++ = esDescr.size();
    memcpy(curBuffer, esDescr.data(), esDescr.size());
    curBuffer += esDescr.size();
    while ((curBuffer - dstBuffer)%4 != 0)
        *curBuffer++ = 0;
    //return len * sizeof(quint32);
    
    return curBuffer - dstBuffer;
}

void RTPSession::processRtcpData(const RtspStatistic* stats)
{
    quint8 rtcpBuffer[MAX_RTCP_PACKET_SIZE];
    quint8 sendBuffer[MAX_RTCP_PACKET_SIZE];
    while (m_rtcpUdpSock.hasData())
    {
        std::string lastReceivedAddr;
        unsigned short lastReceivedPort;
        int readed = m_rtcpUdpSock.recvFrom(rtcpBuffer, sizeof(rtcpBuffer), lastReceivedAddr, lastReceivedPort);
        if (readed > 0)
        {
            if (!m_rtcpUdpSock.isConnected())
            {
                
                m_rtcpUdpSock.setDestAddr(lastReceivedAddr, lastReceivedPort);
            }

            quint32 timestamp = 0;
            double nptTime = 0;
            if (stats) 
            {
                int outBufSize = buildRTCPReport(sendBuffer, stats);
                if (outBufSize > 0)
                {
                    m_rtcpUdpSock.sendTo(sendBuffer, outBufSize);
                }
            }
        }
    }
}

bool RTPSession::sendKeepAliveIfNeeded(const RtspStatistic* stats)
{
    processRtcpData(stats);
    // send rtsp keep alive
    if (m_TimeOut==0)
        return true;

    if (m_keepAliveTime.elapsed() < m_TimeOut - RESERVED_TIMEOUT_TIME)
        return true;
    else 
    {
        bool res= sendKeepAlive();
        m_keepAliveTime.restart();
        return res;
    }
}

bool RTPSession::sendKeepAlive()
{

    QByteArray request;
    QByteArray responce;
    request += "GET_PARAMETER ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    //if (!d->sessionId.isEmpty()) 
    {
        request += "\r\n";
        request += "Session: ";
        request += m_SessionId;
    }
    request += "\r\n\r\n";
    //


    if (!m_tcpSock.send(request.data(), request.size()))
        false;


    if(!readTextResponce(responce) || !responce.startsWith("RTSP/1.0 200"))
    {
        return false;
    }
    else 
    {
        return true;
    }
}

// read RAW: combination of text and binary data
int RTPSession::readRAWData()
{
    int readed = m_tcpSock.recv(m_responseBuffer + m_responseBufferLen, MAX_RESPONCE_LEN - m_responseBufferLen);
    if (readed > 0)
        m_responseBufferLen += readed;
    return readed;
}

// demux binary data only
int RTPSession::readBinaryResponce(quint8* data, int maxDataSize)
{
    bool readMoreData = false; // try to process existing buffer at first
    while (1)
    {
        if (readMoreData && !readRAWData())
            return -1;
        int demuxedCount = 0;
        quint8* curPtr = m_responseBuffer;
        for(; curPtr < m_responseBuffer + m_responseBufferLen; curPtr++)
        {
            if (*curPtr == '$') // start of binary data
            {
                if (m_responseBuffer + m_responseBufferLen - curPtr < 4)
                {
                    m_responseBufferLen = 0;
                    return 0; // corrupt data. just clear buffer
                }
                quint16 dataLen = (curPtr[2] << 8) + curPtr[3];
                if (maxDataSize < dataLen)
                {
                    qWarning() << "Too low RTSP receiving buffer. Data can't be demuxed";
                    m_responseBufferLen = 0;
                    return -1;
                }
                int dataRest = m_responseBuffer + m_responseBufferLen - curPtr;
                if (dataLen <= dataRest) 
                {
                    memcpy(data, curPtr, dataLen);
                    data += dataLen;
                    demuxedCount += dataLen;
                    maxDataSize -= dataLen;

                    memmove(curPtr, curPtr + dataLen, m_responseBuffer + m_responseBufferLen - (curPtr+dataLen));
                    m_responseBufferLen -= dataLen;
                    
                    return demuxedCount;
                }
            }
        }
        readMoreData = true;
    }
}

// demux text data only
bool RTPSession::readTextResponce(QByteArray& response)
{
    bool readMoreData = false; // try to process existing buffer at first
    while (1)
    {
        if (readMoreData && !readRAWData())
            return false;

        quint8* startPtr = m_responseBuffer;
        quint8* curPtr = m_responseBuffer;
        for(; curPtr < m_responseBuffer + m_responseBufferLen; curPtr++)
        {
            if (*curPtr == '$') // start of binary data
            {
                response.append(QByteArray::fromRawData((char*)startPtr, curPtr - startPtr));
                memmove(startPtr, curPtr, m_responseBufferLen - (curPtr - startPtr));
                m_responseBufferLen -= curPtr - startPtr;
                if (!response.isEmpty())
                    return true;

                int dataRest = m_responseBuffer + m_responseBufferLen - startPtr;
                if (dataRest < 4)
                {
                    readMoreData = true;
                    continue;
                }
                quint16 dataLen = (curPtr[2] << 8) + curPtr[3];
                if (dataRest < dataLen)
                {
                    readMoreData = true;
                    continue;
                }
                startPtr += dataLen;
                curPtr = startPtr;
            }
        }
        response.append(QByteArray::fromRawData((char*)startPtr, curPtr - startPtr));
        memmove(startPtr, curPtr, m_responseBufferLen - (curPtr - startPtr));
        m_responseBufferLen -= curPtr - startPtr;
        if (!response.isEmpty())
            return true;
        readMoreData = true;
    }
    return false;
}

QString RTPSession::extractRTSPParam(const QString& buffer, const QString& paramName)
{
    int pos = buffer.indexOf(paramName);
    if (pos > 0)
    {
        QString rez;
        bool first = true;
        for (int i = pos + paramName.size() + 1; i < buffer.size(); i++)
        {
            if (buffer[i] == ' ' && first)
                continue;
            else if (buffer[i] == '\r' || buffer[i] == '\n')
                break;
            else {
                rez += buffer[i];
                first = false;
            }
        }
        return rez;
    }
    else
        return "";
}

void RTPSession::updateTransportHeader(QByteArray& responce)
{
    QString tmp = extractRTSPParam(responce, "Transport:");
    if (tmp.size() > 0) 
    {
        QStringList tmpList = tmp.split(';');
        for (int i = 0; i < tmpList.size(); ++i)
        {
            if (tmpList[i].startsWith("port"))
            {
                QStringList tmpParams = tmpList[i].split('=');
                if (tmpParams.size() > 1)
                    m_ServerPort = tmpParams[1].toInt();
            }
        }
    }
}

void RTPSession::setTransport(const QString& transport)
{
    m_transport = transport;
    m_rtpIo.setTCPMode(m_transport == "TCP");

}

QString RTPSession::getTrackFormat(int trackNum) const
{
    QMap<int, QString>::const_iterator itr = m_sdpTracks.find(trackNum);
    return itr != m_sdpTracks.end() ? itr.value() : QString();
}
