#include <QFile>
#include "rtpsession.h"
#include "rtp_stream_parser.h"

#if defined(Q_OS_WIN)
#  include <winsock2.h>
#endif
#include "utils/common/util.h"

#define DEFAULT_RTP_PORT 554
#define RESERVED_TIMEOUT_TIME (5*1000)


static const int MAX_RTCP_PACKET_SIZE = 1024 * 2;
static const quint32 SSRC_CONST = 0x2a55a9e8;
static const quint32 CSRC_CONST = 0xe8a9552a;
static const int TCP_TIMEOUT = 5 * 1000 * 1000;

//#define DEBUG_RTSP

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

qint64 RTPIODevice::read(char *data, qint64 maxSize)
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
    m_transport("UDP"),
    m_selectedAudioChannel(0),
    m_startTime(AV_NOPTS_VALUE),
    m_endTime(AV_NOPTS_VALUE),
    m_scale(1.0)
{
    m_udpSock.setReadTimeOut(500);
    m_responseBuffer = new quint8[MAX_RESPONCE_LEN];
    m_responseBufferLen = 0;
}

RTPSession::~RTPSession()
{
    m_tcpSock.close();
    delete [] m_responseBuffer;
}

void RTPSession::parseSDP()
{
    QList<QByteArray> lines = m_sdp.split('\n');

    int mapNum = -1;
    QString codecName;
    QString codecType;
    int trackNum = -1;
    foreach(QByteArray line, lines)
    {
        line = line.trimmed().toLower();
        if (line.startsWith("m="))
        {
            if (trackNum >= 0) {
                m_sdpTracks.insert(trackNum, SDPTrackInfo(codecName, codecType));
                trackNum = -1;
            }
            codecType = line.mid(2).split(' ')[0];
        }
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
            trackNum = line.mid(QString("a=control:trackid=").length()).toUInt();
    }
    if (trackNum >= 0)
        m_sdpTracks.insert(trackNum, SDPTrackInfo(codecName, codecType));
}

void RTPSession::parseRangeHeader(const QString& rangeStr)
{
    QStringList rangeType = rangeStr.trimmed().split("=");
    if (rangeType.size() < 2)
        return;
    if (rangeType[0] == "npt")
    {
        QStringList values = rangeType[1].split("-");

        double val = values[0].toDouble();
        m_startTime = val < 1000000 ? val * 1000000.0 : val;
        if (values.size() > 1) 
        {
            if (values[1] == "now") {
                m_endTime = DATETIME_NOW;
            }
            else {
                val = values[1].toDouble();
                m_endTime = val < 1000000 ? val * 1000000.0 : val;
            }
        }
    }
}

bool RTPSession::open(const QString& url)
{
    mUrl = url;

    //unsigned int port = DEFAULT_RTP_PORT;

    if (m_tcpSock.isClosed())
        m_tcpSock.reopen();

    if (!m_tcpSock.connect(mUrl.host().toLatin1().data(), mUrl.port(DEFAULT_RTP_PORT)))
        return false;

    m_tcpSock.setReadTimeOut(TCP_TIMEOUT);
    m_tcpSock.setWriteTimeOut(TCP_TIMEOUT);

    if (!sendDescribe())
        return false;

    QByteArray response;

    if (!readTextResponce(response))
        return false;

    QString tmp = extractRTSPParam(response, "Range:");
    if (!tmp.isEmpty())
    {
        parseRangeHeader(tmp);
    }

    int sdp_index = response.indexOf("\r\n\r\n");

    if (sdp_index  < 0 || sdp_index+4 >= response.size())
        return false;

    m_sdp = response.mid(sdp_index+4);
    parseSDP();

    if (m_sdpTracks.size()<=0)
        return false;

    return true;
}

RTPIODevice* RTPSession::play(qint64 position, double scale)
{
    RTPIODevice* ioDevice = sendSetup();

    if (ioDevice == 0)
        return 0;


    if (!sendPlay(position, scale))
        return 0;


    return &m_rtpIo;

}

bool RTPSession::stop()
{
    //delete m_tcpSock;
    //m_tcpSock = 0;
    m_tcpSock.close();
    m_responseBufferLen = 0;
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

    int audioNum = 0;
    if (m_transport == "UDP")
        m_rtpIo.setSocket(&m_udpSock);
    else
        m_rtpIo.setSocket(&m_tcpSock);
    for (QMap<int, SDPTrackInfo>::iterator itr = m_sdpTracks.begin(); itr != m_sdpTracks.end(); ++itr)
    {
        if (getTrackType(itr.key()) == "audio") 
        {
            if (audioNum++ != m_selectedAudioChannel)
                continue;
        }

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

bool RTPSession::sendPlay(qint64 position, double scale)
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
    if (position != DATETIME_NOW)
        request += "Range: npt=" + QString::number(position);
    else
        request += "Range: npt=now";
    request += "-\r\n";

    request += "Scale: " + QString::number(scale) + QString("\r\n");
    
    request += "\r\n";

    if (!m_tcpSock.send(request.data(), request.size()))
        return false;


    if (!readTextResponce(responce))
        return false;


    if (responce.startsWith("RTSP/1.0 200"))
    {
        updateTransportHeader(responce);
        m_keepAliveTime.restart();
        m_scale = scale;
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
        QString lastReceivedAddr;
        unsigned short lastReceivedPort;
        int readed = m_rtcpUdpSock.recvFrom(rtcpBuffer, sizeof(rtcpBuffer), lastReceivedAddr, lastReceivedPort);
        if (readed > 0)
        {
            if (!m_rtcpUdpSock.isConnected())
            {

                m_rtcpUdpSock.setDestAddr(lastReceivedAddr, lastReceivedPort);
            }

            //quint32 timestamp = 0;
            //double nptTime = 0;
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
    {
        m_responseBufferLen += readed;
        Q_ASSERT( m_responseBufferLen <= MAX_RESPONCE_LEN);
    }
    return readed;
}

// demux binary data only
int RTPSession::readBinaryResponce(quint8* data, int maxDataSize)
{
    bool readMore = false;
    while (m_tcpSock.isConnected())
    {
        if (readMore) {
             int readed = readRAWData();
             if (readed <= 0)
                return readed;
        }

        quint8* bEnd = m_responseBuffer + m_responseBufferLen;

        for(quint8* curPtr = m_responseBuffer; curPtr < bEnd-4; curPtr++)
        {
            if (*curPtr == '$') // start of binary data
            {
                int dataRest = bEnd - curPtr;
                quint16 dataLen = (curPtr[2] << 8) + curPtr[3] + 4;
                if (maxDataSize < dataLen)
                {
                    qWarning("Too low RTSP receiving buffer. Data can't be demuxed");
                    m_responseBufferLen = 0;
                    return -1;
                }

                if (dataRest < dataLen)
                    break;

                memcpy(data, curPtr, dataLen);
                memmove(curPtr, curPtr + dataLen, dataRest - dataLen);
                m_responseBufferLen -= dataLen;

                return dataLen;
            }
        }
        readMore = true;
    }
    return -1;
}

// demux text data only
bool RTPSession::readTextResponce(QByteArray& response)
{

    bool readMoreData = false; // try to process existing buffer at first
    for (int k = 0; k < 10; ++k) // if binary data ahead text data, read more. read 10 packets at maxumum
    {
        if (readMoreData && readRAWData() == -1)
            return false;

        quint8* startPtr = m_responseBuffer;
        quint8* curPtr = m_responseBuffer;
        for(; curPtr < m_responseBuffer + m_responseBufferLen;)
        {
            if (*curPtr == '$') // start of binary data, skip it
            {
                response.append(QByteArray::fromRawData((char*)startPtr, curPtr - startPtr));
                memmove(startPtr, curPtr, (m_responseBuffer+m_responseBufferLen) - curPtr);
                m_responseBufferLen -= curPtr - startPtr;
                if (!response.isEmpty())
                {
                    return true;
                }

                int dataRest = m_responseBuffer + m_responseBufferLen - startPtr;
                if (dataRest < 4)
                {
                    readMoreData = true;
                    startPtr = 0;
                    break;
                }
                quint16 dataLen = (curPtr[2] << 8) + curPtr[3] + 4;
                if (dataRest < dataLen)
                {
                    readMoreData = true;
                    startPtr = 0;
                    break;
                }
                startPtr += dataLen;
                curPtr = startPtr;
            }
            else {
                curPtr++;
            }
        }
        if (startPtr)
        {
            response.append(QByteArray::fromRawData((char*)startPtr, curPtr - startPtr));
            memmove(startPtr, curPtr, (m_responseBuffer+m_responseBufferLen) - curPtr);

            m_responseBufferLen -= curPtr - startPtr;
            if (!response.isEmpty()) {
                return true;
            }
        }
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
    return m_sdpTracks.value(trackNum).codecName;
}

QString RTPSession::getTrackType(int trackNum) const
{
    return m_sdpTracks.value(trackNum).codecType;
}

qint64 RTPSession::startTime() const
{
    return m_startTime;
}

qint64 RTPSession::endTime() const
{
    return m_endTime;
}

float RTPSession::getScale() const
{
    return m_scale;
}
