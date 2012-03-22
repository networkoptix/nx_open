#include <QFile>
#include "rtpsession.h"
#include "rtp_stream_parser.h"

#if defined(Q_OS_WIN)
#  include <winsock2.h>
#endif
#include "utils/common/util.h"
#include "../common/sleep.h"
#include "tcp_connection_processor.h"
#include "simple_http_client.h"

#define DEFAULT_RTP_PORT 554
#define RESERVED_TIMEOUT_TIME (5*1000)


static const int MAX_RTCP_PACKET_SIZE = 1024 * 2;
static const quint32 SSRC_CONST = 0x2a55a9e8;
static const quint32 CSRC_CONST = 0xe8a9552a;

static const int TCP_CONNECT_TIMEOUT = 1000*3;

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
    m_scale(1.0),
    m_tcpTimeout(50 * 1000 * 1000)
{
    m_udpSock.setReadTimeOut(500);
    m_responseBuffer = new quint8[RTSP_BUFFER_LEN];
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
    QByteArray setupURL;

    foreach(QByteArray line, lines)
    {
        line = line.trimmed();
        QByteArray lineLower = line.toLower();
        if (lineLower.startsWith("m="))
        {
            if (trackNum >= 0) {
                m_sdpTracks.insert(trackNum, SDPTrackInfo(codecName, codecType, setupURL));
                trackNum = -1;
                setupURL.clear();
            }
            codecType = lineLower.mid(2).split(' ')[0];
        }
        if (lineLower.startsWith("a=rtpmap"))
        {
            QList<QByteArray> params = lineLower.split(' ');
            if (params.size() < 2)
                continue; // invalid data format. skip
            QList<QByteArray> trackInfo = params[0].split(':');
            QList<QByteArray> codecInfo = params[1].split('/');
            if (trackInfo.size() < 2 || codecInfo.size() < 2)
                continue; // invalid data format. skip
            mapNum = trackInfo[1].toUInt();
            codecName = codecInfo[0];
        }
        //else if (lineLower.startsWith("a=control:track"))
        else if (lineLower.startsWith("a=control:"))
        {
            QByteArray lineRest = line.mid(QByteArray("a=control:").length());
            if (lineLower.startsWith("a=control:track"))
            {
                int i = 0;
                for (; i < lineRest.size() && !(lineRest[i] >= '0' && lineRest[i] <= '9'); ++i);
                setupURL = lineRest.left(i);
                trackNum = lineRest.mid(i).toUInt();
            }
            else if (lineLower.startsWith("a=control:rtsp"))
            {
                int trackStart = lineRest.lastIndexOf('/')+1;
                if (trackStart > 0 && lineRest.mid(trackStart).toLower().startsWith("trackid"))
                {
                    int i = trackStart;
                    for (; i < lineRest.size() && !(lineRest[i] >= '0' && lineRest[i] <= '9'); ++i);
                    setupURL = lineRest; //lineRest.mid(trackStart, i - trackStart);
                    if (lineRest.indexOf('?') != -1)
                        lineRest = lineRest.left(lineRest.indexOf('?'));
                    trackNum = lineRest.mid(i).toUInt();
                }
            }
        }
    }
    if (trackNum >= 0)
        m_sdpTracks.insert(trackNum, SDPTrackInfo(codecName, codecType, setupURL));
}

void RTPSession::parseRangeHeader(const QString& rangeStr)
{
    QStringList rangeType = rangeStr.trimmed().split(QChar('='));
    if (rangeType.size() < 2)
        return;
    if (rangeType[0] == "npt")
    {
        int index = rangeType[1].lastIndexOf(QChar('-'));
        QString start = rangeType[1].mid(0, index);
        QString end = rangeType[1].mid(index + 1);
        if(start.endsWith(QChar('-'))) {
            start = start.left(start.size() - 1);
            end = QChar('-') + end;
        }
        if (start == "now") {
            m_startTime= DATETIME_NOW;
        }
        else {
            double val = start.toDouble();
            m_startTime = val < 1000000 ? val * 1000000.0 : val;
        }
        if (index > 0) 
        {
            if (end == "now") {
                m_endTime = DATETIME_NOW;
            }
            else {
                double val = end.toDouble();
                m_endTime = val < 1000000 ? val * 1000000.0 : val;
            }
        }
    }
}

bool RTPSession::open(const QString& url)
{
    mUrl = url;
    m_responseBufferLen = 0;

    //unsigned int port = DEFAULT_RTP_PORT;

    if (m_tcpSock.isClosed())
        m_tcpSock.reopen();

    m_tcpSock.setReadTimeOut(TCP_CONNECT_TIMEOUT);
    if (!m_tcpSock.connect(mUrl.host().toLatin1().data(), mUrl.port(DEFAULT_RTP_PORT)))
        return false;

    m_tcpSock.setReadTimeOut(m_tcpTimeout);
    m_tcpSock.setWriteTimeOut(m_tcpTimeout);

    m_tcpSock.setNoDelay(true);

    if (!sendDescribe())
        return false;

    QByteArray response;

    if (!readTextResponce(response))
        return false;

    QString tmp = extractRTSPParam(response, "Range:");
    if (!tmp.isEmpty())
        parseRangeHeader(tmp);

    int sdp_index = response.indexOf("\r\n\r\n");

    if (sdp_index  < 0 || sdp_index+4 >= response.size())
        return false;

    m_sdp = response.mid(sdp_index+4);
    parseSDP();

    if (m_sdpTracks.size()<=0)
        return false;

    return true;
}

RTPIODevice* RTPSession::play(qint64 positionStart, qint64 positionEnd, double scale)
{
    RTPIODevice* ioDevice = sendSetup();

    if (ioDevice == 0)
        return 0;


    if (!sendPlay(positionStart, positionEnd, scale))
        return 0;


    return &m_rtpIo;

}

bool RTPSession::stop()
{
    //delete m_tcpSock;
    //m_tcpSock = 0;
    m_tcpSock.close();
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

void RTPSession::addAuth(QByteArray& request)
{
    if (m_auth.isNull())
        return;
    request.append(CLSimpleHTTPClient::basicAuth(m_auth));
    request.append("\r\n");
}   


bool RTPSession::sendDescribe()
{
    QByteArray request;
    request += "DESCRIBE ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    addAuth(request);
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
    request += "\r\n";
    addAuth(request);
    request += "\r\n";


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
        if (getTrackType(itr.key()) == "audio" || m_transport == "UDP") 
        {
            // todo: audio (multi tracks transfer) does not supported for UDP now
            if (audioNum++ != m_selectedAudioChannel)
                continue;
        }
        else if (getTrackType(itr.key()) != "video")
        {
            continue; // skip metadata e.t.c
        }

        QByteArray request;
        request += "SETUP ";

        if (itr.value().setupURL.startsWith("rtsp://"))
        {
            // full track url in a prefix
            request += itr.value().setupURL;
        }   
        else {
            request += mUrl.toString();
            request += '/';
            if (itr.value().setupURL.isEmpty())
                request += QString("trackID=");
            else
                request += itr.value().setupURL;
            request += QByteArray::number(itr.key());
        }

        request += " RTSP/1.0\r\n";
        request += "CSeq: ";
        request += QByteArray::number(m_csec++);
        request += "\r\n";
        addAuth(request);
        request += "User-Agent: Network Optix\r\n";
        request += QString("Transport: RTP/AVP/") + m_transport + QString(";unicast;");

        if (m_transport == "UDP")
        {
            request += "client_port=";
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
                tmpList[i] = tmpList[i].trimmed().toLower();
                if (tmpList[i].startsWith("timeout"))
                {
                    QStringList tmpParams = tmpList[i].split('=');
                    if (tmpParams.size() > 1) {
                        m_TimeOut = tmpParams[1].toInt();
                        if (m_TimeOut > 0 && m_TimeOut < 5000)
                            m_TimeOut *= 1000; // convert seconds to ms
                    }
                }
            }
        }

        updateTransportHeader(responce);
    }

    return &m_rtpIo;
}

void RTPSession::addAdditionAttrs(QByteArray& request)
{
    for (QMap<QByteArray, QByteArray>::const_iterator i = m_additionAttrs.begin(); i != m_additionAttrs.end(); ++i)
    {
        request += i.key();
        request += ": ";
        request += i.value();
        request += "\r\n";
    }
}

bool RTPSession::sendSetParameter(const QByteArray& paramName, const QByteArray& paramValue)
{
    QByteArray request;
    QByteArray requestBody;
    QByteArray responce;

    requestBody.append(paramName);
    requestBody.append(": ");
    requestBody.append(paramValue);
    requestBody.append("\r\n");

    request += "SET_PARAMETER ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    addAuth(request);
    request += "Session: ";
    request += m_SessionId;
    request += "\r\n";
    if (!requestBody.isEmpty())
    {
        request += "Content-Length: ";
        request += QByteArray::number(requestBody.size());
        request += "\r\n";
    }
    addAdditionAttrs(request);

    request += "\r\n";

    if (!requestBody.isEmpty())
    {
        request += requestBody;
    }

    if (!m_tcpSock.send(request.data(), request.size()))
        return false;


    if (!readTextResponce(responce))
        return false;


    if (responce.startsWith("RTSP/1.0 200"))
    {
        m_keepAliveTime.restart();
        return true;
    }

    return false;
}

bool RTPSession::sendPlay(qint64 startPos, qint64 endPos, double scale)
{
    QByteArray request;
    QByteArray response;

    m_scale = scale;

    request += "PLAY ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    addAuth(request);
    request += "Session: ";
    request += m_SessionId;
    request += "\r\n";
    if (startPos != AV_NOPTS_VALUE)
    {
        if (startPos != DATETIME_NOW)
            request += "Range: npt=" + QString::number(startPos);
        else
            request += "Range: npt=now";
        request += "-";
        if (endPos != AV_NOPTS_VALUE)
        {
            if (endPos != DATETIME_NOW)
                request += QString::number(endPos);
            else
                request += "now";
        }
        request += "\r\n";
    }

    request += "Scale: " + QString::number(scale) + QString("\r\n");
    addAdditionAttrs(request);
    
    request += "\r\n";

    if (!m_tcpSock.send(request.data(), request.size()))
        return false;


    if (!readTextResponce(response))
        return false;


    if (response.startsWith("RTSP/1.0 200"))
    {
        updateTransportHeader(response);

        QString tmp = extractRTSPParam(response, "Range:");
        if (!tmp.isEmpty())
            parseRangeHeader(tmp);

        m_keepAliveTime.restart();
        return true;
    }

    return false;

}

bool RTPSession::sendPause()
{

    QByteArray request;
    QByteArray response;

    request += "PAUSE ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    addAuth(request);
    request += "Session: ";
    request += m_SessionId;
    request += "\r\n";

    request += "\r\n";

    if (!m_tcpSock.send(request.data(), request.size()))
        return false;


    if (!readTextResponce(response))
        return false;


    if (response.startsWith("RTSP/1.0 200"))
    {
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
    addAuth(request);
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
    *curBuffer++ = 4; // len field = 6 (low), (4+1)*4=20 bytes
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
        addAuth(request);
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
    int readed = m_tcpSock.recv(m_responseBuffer + m_responseBufferLen, qMin(RTSP_BUFFER_LEN - m_responseBufferLen, MAX_RTSP_DATA_LEN));
    if (readed > 0)
    {
        m_responseBufferLen += readed;
        Q_ASSERT( m_responseBufferLen <= RTSP_BUFFER_LEN);
    }
    return readed;
}

// demux binary data only
int RTPSession::readBinaryResponce(quint8* data, int maxDataSize)
{
    bool readMore = false;
    QByteArray textResponse;
    while (m_tcpSock.isConnected())
    {
        if (readMore) {
             int readed = readRAWData();
             if (readed <= 0)
                return readed;
        }

        quint8* bEnd = m_responseBuffer + m_responseBufferLen;

        //for(quint8* curPtr = m_responseBuffer; curPtr < bEnd-4; curPtr++)
        quint8* curPtr = m_responseBuffer;

        if (*curPtr != '$') 
        {
            // text Data
            for(; curPtr < bEnd && *curPtr != '$'; curPtr++);
            textResponse.append((const char*) m_responseBuffer, curPtr - m_responseBuffer);
            memmove(m_responseBuffer, curPtr, bEnd - curPtr);
            m_responseBufferLen = bEnd - curPtr;
            if (QnTCPConnectionProcessor::isFullMessage(textResponse)) 
            {
                QString tmp = extractRTSPParam(textResponse, "Range:");
                if (!tmp.isEmpty())
                    parseRangeHeader(tmp);
                emit gotTextResponse(textResponse);
                textResponse.clear();
                continue;
            }
        }
        else // start of binary data
        {
            int dataRest = bEnd - curPtr;
            quint16 dataLen = (curPtr[2] << 8) + curPtr[3] + 4;
            if (maxDataSize < dataLen)
            {
                qWarning("Too low RTSP receiving buffer. Data can't be demuxed");
                m_responseBufferLen = 0;
                return -1;
            }

            if (dataRest >= dataLen)
            {
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
    int retry_count = 40;
    for (int i = 0; i < retry_count; ++i)
    {
        bool readMoreData = false; // try to process existing buffer at first
        //for (int k = 0; k < 10; ++k) // if binary data ahead text data, read more. read 10 packets at maxumum
        while (m_responseBufferLen < RTSP_BUFFER_LEN)
        {
            if (readMoreData && readRAWData() <= 0)
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
                    if (QnTCPConnectionProcessor::isFullMessage(response))
                        return true;

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
                if (QnTCPConnectionProcessor::isFullMessage(response))
                    return true;
            }
            readMoreData = true;
            QnSleep::msleep(1);
        }

        // buffer is full and we does not find text data (buffer is filled by binary data). Clear buffer (and drop some video frames) and try to find data again
        qWarning() << "Can't find RTSP text response because of buffer is full! Drop some media data and find againg";
        quint8 tmpBuffer[1024*64];
        if (readBinaryResponce(tmpBuffer, sizeof(tmpBuffer)) == -1)
            break;
    }
    qWarning() << "RTSP Text response not found! RTSP command is failed!";
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

void RTPSession::setScale(float value)
{
    m_scale = value;
}

void RTPSession::setAdditionAttribute(const QByteArray& name, const QByteArray& value)
{
    m_additionAttrs.insert(name, value);
}

void RTPSession::removeAdditionAttribute(const QByteArray& name)
{
    m_additionAttrs.remove(name);
}

void RTPSession::setTCPTimeout(int timeout)
{
    m_tcpTimeout = timeout;
}

void RTPSession::setAuth(const QAuthenticator& auth)
{
    m_auth = auth;
}

QAuthenticator RTPSession::getAuth() const
{
    return m_auth;
}
