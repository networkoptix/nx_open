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
#include "utils/media/bitStream.h"
#include "../common/synctime.h"
#include "tcp_connection_priv.h"

#define DEFAULT_RTP_PORT 554
#define RESERVED_TIMEOUT_TIME (5*1000)

static const int MAX_RTCP_PACKET_SIZE = 1024 * 2;
static const quint32 SSRC_CONST = 0x2a55a9e8;
static const quint32 CSRC_CONST = 0xe8a9552a;

static const int TCP_CONNECT_TIMEOUT = 1000*2;
static const int SDP_TRACK_STEP = 2;
static const int METADATA_TRACK_NUM = 7;

//#define DEBUG_RTSP

static QString getValueFromString(const QString& line)
{
    int index = line.indexOf(QLatin1Char('='));
    if (index < 1)
        return QString();
    return line.mid(index+1);
}

// --------------------- RTPIODevice --------------------------

RTPIODevice::RTPIODevice(RTPSession* owner, bool useTCP):
    m_owner(owner),
    m_tcpMode(false),
    m_mediaSocket(0),
    m_rtcpSocket(0)
{
    m_tcpMode = useTCP;
    if (!m_tcpMode) 
    {
        m_mediaSocket = new UDPSocket(0);
        m_mediaSocket->setReadTimeOut(500);

        m_rtcpSocket = new UDPSocket(0);
        m_rtcpSocket->setReadTimeOut(500);
    }
}

RTPIODevice::~RTPIODevice()
{
    delete m_mediaSocket;
    delete m_rtcpSocket;
}


/*
void RTPIODevice::setTCPSocket(CommunicatingSocket* socket) 
{
    m_sock = socket; 
    m_tcpMode = true;
}
*/

qint64 RTPIODevice::read(char *data, qint64 maxSize)
{

    int readed;
    if (m_tcpMode)
        readed = m_owner->readBinaryResponce((quint8*) data, maxSize); // demux binary data from TCP socket
    else
        readed = m_mediaSocket->recv(data, maxSize);
    m_owner->sendKeepAliveIfNeeded();
    if (!m_tcpMode)
        processRtcpData();
    return readed;
}

CommunicatingSocket* RTPIODevice::getMediaSocket()
{ 
    if (m_tcpMode) 
        return &m_owner->m_tcpSock;
    else
        return m_mediaSocket; 
}

void RTPIODevice::setTcpMode(bool value)
{ 
    m_tcpMode = value; 
    if (m_tcpMode && m_mediaSocket) {
        m_mediaSocket->close();
        m_rtcpSocket->close();
    }
}

void RTPIODevice::processRtcpData()
{
    quint8 rtcpBuffer[MAX_RTCP_PACKET_SIZE];
    quint8 sendBuffer[MAX_RTCP_PACKET_SIZE];
    while (m_rtcpSocket->hasData())
    {
        QString lastReceivedAddr;
        unsigned short lastReceivedPort;
        int readed = m_rtcpSocket->recvFrom(rtcpBuffer, sizeof(rtcpBuffer), lastReceivedAddr, lastReceivedPort);
        if (readed > 0)
        {
            if (!m_rtcpSocket->isConnected())
            {
                if (!m_rtcpSocket->setDestAddr(lastReceivedAddr, lastReceivedPort))
                {
                    qWarning() << "RTPIODevice::processRtcpData(): setDestAddr() failed: " << m_rtcpSocket->lastError();
                }
            }
            m_statistic = m_owner->parseServerRTCPReport(rtcpBuffer, readed);
            int outBufSize = m_owner->buildClientRTCPReport(sendBuffer);
            if (outBufSize > 0)
            {
                m_rtcpSocket->sendTo(sendBuffer, outBufSize);
            }
        }
    }
}

// ================================================== QnRtspTimeHelper ==========================================

QnRtspTimeHelper::QnRtspTimeHelper():
    m_cameraClockToLocalDiff(INT_MAX)
{

}

double QnRtspTimeHelper::cameraTimeToLocalTime(double cameraTime)
{
    if (m_cameraClockToLocalDiff == INT_MAX)  
        m_cameraClockToLocalDiff = qnSyncTime->currentMSecsSinceEpoch()/1000.0 - cameraTime;
    return cameraTime + m_cameraClockToLocalDiff;
}

void QnRtspTimeHelper::reset()
{
    m_cameraClockToLocalDiff = INT_MAX;
}

qint64 QnRtspTimeHelper::getUsecTime(quint32 rtpTime, const RtspStatistic& statistics, int frequency, bool recursiveAllowed)
{
    if (statistics.isEmpty())
        return qnSyncTime->currentMSecsSinceEpoch() * 1000;
    else {
        int rtpTimeDiff = rtpTime - statistics.timestamp;
        double resultInSecs = cameraTimeToLocalTime(statistics.nptTime + rtpTimeDiff / double(frequency));
        double localTimeInSecs = qnSyncTime->currentMSecsSinceEpoch()/1000.0;
        if (qAbs(localTimeInSecs - resultInSecs) < MAX_FRAME_DURATION/1000 || !recursiveAllowed)
            return resultInSecs * 1000000ll;
        else {
            reset();
            return getUsecTime(rtpTime, statistics, frequency, false);
        }
    }
}


// ================================================== RTPSession ==========================================
RTPSession::RTPSession():
    m_csec(2),
    //m_rtpIo(*this),
    m_transport(TRANSPORT_UDP),
    m_selectedAudioChannel(0),
    m_startTime(AV_NOPTS_VALUE),
    m_endTime(AV_NOPTS_VALUE),
    m_scale(1.0),
    m_tcpTimeout(50 * 1000 * 1000),
    m_proxyPort(0),
    m_responseCode(CODE_OK),
    m_isAudioEnabled(true),
    m_useDigestAuth(false),
    m_numOfPredefinedChannels(0),
    m_TimeOut(0)
{
    m_responseBuffer = new quint8[RTSP_BUFFER_LEN];
    m_responseBufferLen = 0;

    // todo: debug only remove me
}

RTPSession::~RTPSession()
{
    m_tcpSock.close();
    delete [] m_responseBuffer;
}

int RTPSession::getLastResponseCode() const
{
    return m_responseCode;
}

// see rfc1890 for full RTP predefined codec list
QString findCodecById(int num)
{
    switch (num)
    {
        case 0: return QLatin1String("PCMU");
        case 8: return QLatin1String("PCMA");
        case 26: return QLatin1String("JPEG");
        default: return QString();
    }
}

void RTPSession::usePredefinedTracks()
{
    if(!m_sdpTracks.isEmpty())
        return;

    static const quint8 FFMPEG_CODE = 102;
    static const QString FFMPEG_STR(QLatin1String("FFMPEG"));
    static const quint8 METADATA_CODE = 126;
    static const QString METADATA_STR(QLatin1String("ffmpeg-metadata"));

    int trackNum = 0;
    for (; trackNum < m_numOfPredefinedChannels; ++trackNum)
    {
        m_sdpTracks << QSharedPointer<SDPTrackInfo> (new SDPTrackInfo(FFMPEG_STR, QLatin1String("video"), QString(QLatin1String("trackID=%1")).arg(trackNum), FFMPEG_CODE, trackNum, this, true));
    }

    m_sdpTracks << QSharedPointer<SDPTrackInfo> (new SDPTrackInfo(FFMPEG_STR, QLatin1String("audio"), QString(QLatin1String("trackID=%1")).arg(trackNum), FFMPEG_CODE, trackNum, this, true));
    m_sdpTracks << QSharedPointer<SDPTrackInfo> (new SDPTrackInfo(METADATA_STR, QLatin1String("metadata"), QLatin1String("trackID=7"), METADATA_CODE, METADATA_TRACK_NUM, this, true));
}


void RTPSession::parseSDP()
{
    QList<QByteArray> lines = m_sdp.split('\n');

    int mapNum = -1;
    QString codecName;
    QString codecType;
    QString setupURL;
    int trackNumber = 0;

    foreach(QByteArray line, lines)
    {
        line = line.trimmed();
        QByteArray lineLower = line.toLower();
        if (lineLower.startsWith("m="))
        {
            if (mapNum >= 0) {
                if (codecName.isEmpty())
                    codecName = findCodecById(mapNum);
                m_sdpTracks << QSharedPointer<SDPTrackInfo> (new SDPTrackInfo(codecName, codecType, setupURL, mapNum, trackNumber, this, m_transport == TRANSPORT_TCP));
                setupURL.clear();
                trackNumber++;
            }
            QList<QByteArray> trackParams = lineLower.mid(2).split(' ');
            codecType = QLatin1String(trackParams[0]);
            codecName.clear();
            mapNum = 0;
            if (trackParams.size() >= 4) {
                mapNum = trackParams[3].toInt();
            }
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
            codecName = QLatin1String(codecInfo[0]);
        }
        //else if (lineLower.startsWith("a=control:track"))
        else if (lineLower.startsWith("a=control:"))
        {
            setupURL = QLatin1String(line.mid(QByteArray("a=control:").length()));
        }
    }
    if (mapNum >= 0) {
        if (codecName.isEmpty())
            codecName = findCodecById(mapNum);
        if (codecName == QLatin1String("ffmpeg-metadata"))
            trackNumber = METADATA_TRACK_NUM;
        m_sdpTracks << QSharedPointer<SDPTrackInfo> (new SDPTrackInfo(codecName, codecType, setupURL, mapNum, trackNumber, this, m_transport == TRANSPORT_TCP));
    }
}

void RTPSession::parseRangeHeader(const QString& rangeStr)
{
    QStringList rangeType = rangeStr.trimmed().split(QLatin1Char('='));
    if (rangeType.size() < 2)
        return;
    if (rangeType[0] == QLatin1String("npt"))
    {
        int index = rangeType[1].lastIndexOf(QLatin1Char('-'));
        QString start = rangeType[1].mid(0, index);
        QString end = rangeType[1].mid(index + 1);
        if(start.endsWith(QLatin1Char('-'))) {
            start = start.left(start.size() - 1);
            end = QLatin1Char('-') + end;
        }
        if (start == QLatin1String("now")) {
            m_startTime= DATETIME_NOW;
        }
        else {
            double val = start.toDouble();
            m_startTime = val < 1000000 ? val * 1000000.0 : val;
        }
        if (index > 0) 
        {
            if (end == QLatin1String("now")) {
                m_endTime = DATETIME_NOW;
            }
            else {
                double val = end.toDouble();
                m_endTime = val < 1000000 ? val * 1000000.0 : val;
            }
        }
    }
}

void RTPSession::updateResponseStatus(const QByteArray& response)
{
    int firstLineEnd = response.indexOf('\n');
    if (firstLineEnd >= 0) {
        QList<QByteArray> params = response.left(firstLineEnd).split(' ');
        if (params.size() >= 2) {
            m_responseCode = params[1].trimmed().toInt();
        }
    }
}

// in case of error return false
bool RTPSession::checkIfDigestAuthIsneeded(const QByteArray& response)
{
    QString wwwAuth = extractRTSPParam(QLatin1String(response), QLatin1String("WWW-Authenticate:"));

    if (wwwAuth.toLower().contains(QLatin1String("digest")))
    {
        QStringList params = wwwAuth.split(QLatin1Char(','));
        if (params.size()<2)
            return false;

        m_realm = QString();
        m_nonce = QString();

        foreach(QString val, params)
        {
            if (val.contains(QLatin1String("realm")))
                m_realm = getValueFromString(val);
            else if (val.contains(QLatin1String("nonce")))
                m_nonce = getValueFromString(val);
        }

        m_realm.remove(QLatin1Char('"'));
        m_nonce.remove(QLatin1Char('"'));


        if (m_realm.isEmpty() || m_nonce.isEmpty())
            return false;


        m_useDigestAuth = true;
    }
    else
    {
        m_useDigestAuth = false;
    }


    return true;
}

bool RTPSession::open(const QString& url, qint64 startTime)
{
    if (startTime != AV_NOPTS_VALUE)
        m_startTime = startTime;
    m_SessionId.clear();
    m_responseCode = CODE_OK;
    mUrl = url;
    m_contentBase = mUrl.toString();
    m_responseBufferLen = 0;
    m_useDigestAuth = false;



    //unsigned int port = DEFAULT_RTP_PORT;

    if (m_tcpSock.isClosed())
        m_tcpSock.reopen();

    m_tcpSock.setReadTimeOut(TCP_CONNECT_TIMEOUT);
    bool rez;
    if (m_proxyPort == 0)
        rez = m_tcpSock.connect(mUrl.host(), mUrl.port(DEFAULT_RTP_PORT));
    else
        rez = m_tcpSock.connect(m_proxyAddr, m_proxyPort);

    if (!rez)
        return false;

    m_tcpSock.setNoDelay(true);

    m_tcpSock.setReadTimeOut(m_tcpTimeout);
    m_tcpSock.setWriteTimeOut(m_tcpTimeout);

    if (m_numOfPredefinedChannels) {
        usePredefinedTracks();
        return true;
    }

    if (!sendDescribe()) {
        m_tcpSock.close();
        return false;
    }

    QByteArray response;

    if (!readTextResponce(response)) {
        m_tcpSock.close();
        return false;
    }

    // check digest authentication here
    if (!checkIfDigestAuthIsneeded(response))
    {
        m_tcpSock.close();
        return false;
    }
        

    // if digest authentication is nedded need to resend describe 
    if (m_useDigestAuth)
    {
        response.clear();
        if (!sendDescribe() || !readTextResponce(response)) 
        {
            m_tcpSock.close();
            return false;
        }

    }


    QString tmp = extractRTSPParam(QLatin1String(response), QLatin1String("Range:"));
    if (!tmp.isEmpty())
        parseRangeHeader(tmp);

    tmp = extractRTSPParam(QLatin1String(response), QLatin1String("Content-Base:"));
    if (!tmp.isEmpty())
        m_contentBase = tmp;


    updateResponseStatus(response);

    int sdp_index = response.indexOf(QLatin1String("\r\n\r\n"));

    if (sdp_index  < 0 || sdp_index+4 >= response.size()) {
        m_tcpSock.close();
        return false;
    }

    m_sdp = response.mid(sdp_index+4);
    parseSDP();

    if (m_sdpTracks.size()<=0) {
        m_tcpSock.close();
        return false;
    }

    return true;
}

RTPSession::TrackMap RTPSession::play(qint64 positionStart, qint64 positionEnd, double scale)
{
    m_prefferedTransport = m_transport;
    if (m_prefferedTransport == TRANSPORT_AUTO)
        m_prefferedTransport = TRANSPORT_TCP;
    m_TimeOut = 0; // default timeout 0 ( do not send keep alive )
    if (!m_numOfPredefinedChannels) {
        if (!sendSetup() || m_sdpTracks.isEmpty())
            return TrackMap();
    }

    if (!sendPlay(positionStart, positionEnd, scale)) 
        m_sdpTracks.clear();

    return m_sdpTracks;
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
    if (m_useDigestAuth)
    {
        QByteArray firstLine = request.left(request.indexOf('\n'));
        QList<QByteArray> methodAndUri = firstLine.split(' ');
        if (methodAndUri.size() >= 2)
            request.append(CLSimpleHTTPClient::digestAccess(m_auth, m_realm, m_nonce, QLatin1String(methodAndUri[0]), QLatin1String(methodAndUri[1]) ));
    }
    else {
        request.append(CLSimpleHTTPClient::basicAuth(m_auth));
        request.append(QLatin1String("\r\n"));
    }
}   


bool RTPSession::sendDescribe()
{
    m_sdpTracks.clear();
    QByteArray request;
    request += "DESCRIBE ";
    request += mUrl.toString();
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    addAuth(request);
    addRangeHeader(request, m_startTime, AV_NOPTS_VALUE);
    request += "User-Agent: Network Optix\r\n";
    request += "Accept: application/sdp\r\n\r\n";

    //qDebug() << request;

    return (m_tcpSock.send(request.data(), request.size()));
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

    return (m_tcpSock.send(request.data(), request.size()));
}

RTPIODevice* RTPSession::getTrackIoByType(TrackType trackType)
{
    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        if (m_sdpTracks[i]->trackType == trackType)
            return m_sdpTracks[i]->ioDevice;
    }
    return 0;
}

QString RTPSession::getCodecNameByType(TrackType trackType)
{
    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        if (m_sdpTracks[i]->trackType == trackType)
            return m_sdpTracks[i]->codecName;
    }
    return QString();
}

QList<QByteArray> RTPSession::getSdpByType(TrackType trackType) const
{
    QList<QByteArray> rez;
    QList<QByteArray> tmp = m_sdp.split('\n');
    
    int mapNum = -1;
    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        if (m_sdpTracks[i]->trackType == trackType)
            mapNum = m_sdpTracks[i]->mapNum;
    }
    if (mapNum == -1)
        return rez;

    for (int i = 0; i < tmp.size(); ++i)
    {
        QByteArray line = tmp[i].trimmed();
        if (line.startsWith("a=")) {
            QByteArray lineMapNum = line.split(' ')[0];
            lineMapNum = lineMapNum.mid(line.indexOf(':')+1);
            if (lineMapNum.toInt() == mapNum)
                rez << line;
        }
    }
    return rez;
}

bool RTPSession::sendSetup()
{
    int audioNum = 0;

    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        QSharedPointer<SDPTrackInfo> trackInfo = m_sdpTracks[i];

        if (trackInfo->trackType == TT_AUDIO)
        {
            if (!m_isAudioEnabled || audioNum++ != m_selectedAudioChannel)
                continue;
        }
        else if (trackInfo->trackType == TT_VIDEO)
        {
            ;
        }
        else if (trackInfo->codecName != QLatin1String("ffmpeg-metadata"))
        {
            continue; // skip unknown metadata e.t.c
        }

        QByteArray request;
        request += "SETUP ";

        if (trackInfo->setupURL.startsWith(QLatin1String("rtsp://")))
        {
            // full track url in a prefix
            request += trackInfo->setupURL;
        }   
        else {
            request += mUrl.toString();
            request += '/';
            request += trackInfo->setupURL;
            /*
            if (trackInfo->setupURL.isEmpty())
                request += QString("trackID=");
            else
                request += trackInfo->setupURL;
            request += QByteArray::number(itr.key());
            */
        }

        request += " RTSP/1.0\r\n";
        request += "CSeq: ";
        request += QByteArray::number(m_csec++);
        request += "\r\n";
        addAuth(request);
        request += "User-Agent: Network Optix\r\n";
        request += "Transport: RTP/AVP/";
        request += m_prefferedTransport == TRANSPORT_UDP ? "UDP" : "TCP";
        request += ";unicast;";

        if (m_prefferedTransport == TRANSPORT_UDP)
        {
            request += "client_port=";
            request += QString::number(trackInfo->ioDevice->getMediaSocket()->getLocalPort());
            request += '-';
            request += QString::number(trackInfo->ioDevice->getRtcpSocket()->getLocalPort());
        }
        else
        {
            request += QLatin1String("interleaved=") + QString::number(trackInfo->trackNum*SDP_TRACK_STEP) + QLatin1Char('-') + QString::number(trackInfo->trackNum*SDP_TRACK_STEP+1);
        }
        request += "\r\n";

        if (!m_SessionId.isEmpty()) {
            request += "Session: ";
            request += m_SessionId;
            request += "\r\n";
        }


        request += "\r\n";

        //qDebug() << request;

        if (!m_tcpSock.send(request.data(), request.size()))
            return 0;

        QByteArray responce;

        if (!readTextResponce(responce))
            return false;


        if (!responce.startsWith("RTSP/1.0 200"))
        {
            if (m_transport == TRANSPORT_AUTO && m_prefferedTransport == TRANSPORT_TCP) {
                m_prefferedTransport = TRANSPORT_UDP;
                return sendSetup(); // try UDP transport
            }
            else
                return false;
        }

        QString tmp = extractRTSPParam(QLatin1String(responce), QLatin1String("Session:"));

        if (tmp.size() > 0)
        {
            QStringList tmpList = tmp.split(QLatin1Char(';'));
            m_SessionId = tmpList[0];

            for (int i = 0; i < tmpList.size(); ++i)
            {
                tmpList[i] = tmpList[i].trimmed().toLower();
                if (tmpList[i].startsWith(QLatin1String("timeout")))
                {
                    QStringList tmpParams = tmpList[i].split(QLatin1Char('='));
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

    bool tcpMode =  m_prefferedTransport == TRANSPORT_TCP;
    for (int i = 0; i < m_sdpTracks.size(); ++i)
        m_sdpTracks[i]->ioDevice->setTcpMode(tcpMode);

    return true;
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

    return true;

    /*
    if (!readTextResponce(responce))
        return false;


    if (responce.startsWith("RTSP/1.0 200"))
    {
        m_keepAliveTime.restart();
        return true;
    }
    return false;
    */
}

void RTPSession::addRangeHeader(QByteArray& request, qint64 startPos, qint64 endPos)
{
    if (startPos != qint64(AV_NOPTS_VALUE))
    {
        if (startPos != DATETIME_NOW)
            request += QLatin1String("Range: npt=") + QString::number(startPos);
        else
            request += QLatin1String("Range: npt=now");
        request += '-';
        if (endPos != qint64(AV_NOPTS_VALUE))
        {
            if (endPos != DATETIME_NOW)
                request += QString::number(endPos);
            else
                request += QLatin1String("now");
        }
        request += QLatin1String("\r\n");
    }

}

bool RTPSession::sendPlay(qint64 startPos, qint64 endPos, double scale)
{
    QByteArray request;
    QByteArray response;

    m_scale = scale;

    request += "PLAY ";
    request += m_contentBase;
    request += " RTSP/1.0\r\n";
    request += "CSeq: ";
    request += QByteArray::number(m_csec++);
    request += "\r\n";
    addAuth(request);
    request += "Session: ";
    request += m_SessionId;
    request += "\r\n";
    addRangeHeader(request, startPos, endPos);
    request += QLatin1String("Scale: ") + QString::number(scale) + QLatin1String("\r\n");
    if (m_numOfPredefinedChannels)
        request += "x-play-now: true\r\n";
    addAdditionAttrs(request);
    
    request += QLatin1String("\r\n");

    if (!m_tcpSock.send(request.data(), request.size()))
        return false;


    while (1)
    {
        if (!readTextResponce(response))
            return false;

        QString cseq = extractRTSPParam(QLatin1String(response), QLatin1String("CSeq:"));
        if (cseq.toInt() == (int)m_csec-1)
            break;
    }    


    QString tmp = extractRTSPParam(QLatin1String(response), QLatin1String("Range:"));
    if (!tmp.isEmpty())
        parseRangeHeader(tmp);

    if (response.startsWith("RTSP/1.0 200"))
    {
        updateTransportHeader(response);


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


    return true;
    /*
    if (!readTextResponce(response))
        return false;


    if (response.startsWith("RTSP/1.0 200"))
    {
        m_keepAliveTime.restart();
        return true;
    }

    return false;
    */
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
        return false;

    return true;
    //return (readTextResponce(responce) && responce.startsWith("RTSP/1.0 200"));
}

static const int RTCP_SENDER_REPORT = 200;
static const int RTCP_RECEIVER_REPORT = 201;
static const int RTCP_SOURCE_DESCRIPTION = 202;

RtspStatistic RTPSession::parseServerRTCPReport(quint8* srcBuffer, int srcBufferSize)
{
    static quint32 rtspTimeDiff = QDateTime::fromString(QLatin1String("1900-01-01"), Qt::ISODate)
        .secsTo(QDateTime::fromString(QLatin1String("1970-01-01"), Qt::ISODate));

    RtspStatistic stats;
    try {
        BitStreamReader reader(srcBuffer, srcBuffer + srcBufferSize);
        reader.skipBits(8); // skip version

        while (reader.getBitsLeft() > 0)
        {
            int messageCode = reader.getBits(8);
            int messageLen = reader.getBits(16);
            if (messageCode == RTCP_SENDER_REPORT)
            {
                reader.skipBits(32); // sender ssrc
                quint32 intTime = reader.getBits(32);
                quint32 fracTime = reader.getBits(32);
                stats.nptTime = intTime + (double) fracTime / UINT_MAX - rtspTimeDiff;
                stats.timestamp = reader.getBits(32);
                stats.receivedPackets = reader.getBits(32);
                stats.receivedOctets = reader.getBits(32);
                break;
            }
            else {
                reader.skipBits(32 * messageLen);
            }
        }
    } catch(...)
    {
        
    }
    return stats;
}


int RTPSession::buildClientRTCPReport(quint8* dstBuffer)
{
    QByteArray esDescr("netoptix");


    quint8* curBuffer = dstBuffer;
    *curBuffer++ = (RtpHeader::RTP_VERSION << 6);
    *curBuffer++ = RTCP_RECEIVER_REPORT;  // packet type
    curBuffer += 2; // skip len field;

    quint32* curBuf32 = (quint32*) curBuffer;
    *curBuf32++ = htonl(SSRC_CONST);
    /*
    *curBuf32++ = htonl((quint32) stats->nptTime);
    quint32 fracTime = (quint32) ((stats->nptTime - (quint32) stats->nptTime) * UINT_MAX); // UINT32_MAX
    *curBuf32++ = htonl(fracTime);
    *curBuf32++ = htonl(stats->timestamp);
    *curBuf32++ = htonl(stats->receivedPackets);
    *curBuf32++ = htonl(stats->receivedOctets);
    */

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

bool RTPSession::sendKeepAliveIfNeeded()
{
    // send rtsp keep alive
    if (m_TimeOut==0)
        return true;

    if (m_keepAliveTime.elapsed() < (int) m_TimeOut - RESERVED_TIMEOUT_TIME)
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
        return false;
    return true;
    //return (readTextResponce(responce) && responce.startsWith("RTSP/1.0 200"));
}

// read RAW: combination of text and binary data
/*
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
*/

void RTPSession::sendBynaryResponse(quint8* buffer, int size)
{
    m_tcpSock.send(buffer, size);
}


bool RTPSession::processTextResponseInsideBinData()
{
    // have text response or part of text response.
    int readed = m_tcpSock.recv(m_responseBuffer+m_responseBufferLen, qMin(1024, RTSP_BUFFER_LEN - m_responseBufferLen));
    if (readed <= 0)
        return false;
    m_responseBufferLen += readed;

    quint8* curPtr = m_responseBuffer;
    quint8* bEnd = m_responseBuffer+m_responseBufferLen;
    for(; curPtr < bEnd && *curPtr != '$'; curPtr++);
    if (curPtr < bEnd || QnTCPConnectionProcessor::isFullMessage(QByteArray::fromRawData((const char*)m_responseBuffer, m_responseBufferLen)))
    {
        QByteArray textResponse;
        textResponse.append((const char*) m_responseBuffer, curPtr - m_responseBuffer);
        memmove(m_responseBuffer, curPtr, bEnd - curPtr);
        m_responseBufferLen = bEnd - curPtr;
        QString tmp = extractRTSPParam(QLatin1String(textResponse), QLatin1String("Range:"));
        if (!tmp.isEmpty())
            parseRangeHeader(tmp);
        emit gotTextResponse(textResponse);
    }
    return true;
}

int RTPSession::readBinaryResponce(quint8* data, int maxDataSize)
{
    while (m_tcpSock.isConnected())
    {
        while (m_responseBufferLen < 4) {
            int readed = m_tcpSock.recv(m_responseBuffer+m_responseBufferLen, 4 - m_responseBufferLen);
            if (readed <= 0)
                return readed;
            m_responseBufferLen += readed;
        }
        if (m_responseBuffer[0] == '$')
            break;

        // have text response or part of text response.
        if (!processTextResponseInsideBinData())
            return -1;
    }
    int dataLen = (m_responseBuffer[2]<<8) + m_responseBuffer[3] + 4;
    if (maxDataSize < dataLen)
        return -2; // not enough buffer
    int copyLen = qMin(dataLen, m_responseBufferLen);
    memcpy(data, m_responseBuffer, copyLen);
    if (m_responseBufferLen > copyLen)
        memmove(m_responseBuffer, m_responseBuffer + copyLen, m_responseBufferLen - copyLen);
    data += copyLen;
    m_responseBufferLen -= copyLen;
    for (int dataRestLen = dataLen - copyLen; dataRestLen > 0;)
    {
        int readed = m_tcpSock.recv(data, dataRestLen);
        if (readed <= 0)
            return readed;
        dataRestLen -= readed;
        data += readed;
    }
    return dataLen;
}

quint8* RTPSession::prepareDemuxedData(QVector<QnByteArray*>& demuxedData, int channel, int reserve)
{
    if (demuxedData.size() <= channel)
        demuxedData.resize(channel+1);
    if (demuxedData[channel] == 0)
        demuxedData[channel] = new QnByteArray(16, 0);
    QnByteArray* dataVect = demuxedData[channel];
    //dataVect->resize(dataVect->size() + reserve);
    dataVect->reserve(dataVect->size() + reserve);
    return (quint8*) dataVect->data() + dataVect->size();
}

int RTPSession::readBinaryResponce(QVector<QnByteArray*>& demuxedData, int& channelNumber)
{
    while (m_tcpSock.isConnected())
    {
        while (m_responseBufferLen < 4) {
            int readed = m_tcpSock.recv(m_responseBuffer+m_responseBufferLen, 4 - m_responseBufferLen);
            if (readed <= 0)
                return readed;
            m_responseBufferLen += readed;
        }
        if (m_responseBuffer[0] == '$')
            break;
        if (!processTextResponseInsideBinData())
            return -1;
    }

    int dataLen = (m_responseBuffer[2]<<8) + m_responseBuffer[3] + 4;
    int copyLen = qMin(dataLen, m_responseBufferLen);
    channelNumber = m_responseBuffer[1];
    quint8* data = prepareDemuxedData(demuxedData, channelNumber, dataLen);
    //quint8* data = (quint8*) dataVect->data() + dataVect->size() - dataLen;

    memcpy(data, m_responseBuffer, copyLen);
    if (m_responseBufferLen > copyLen)
        memmove(m_responseBuffer, m_responseBuffer + copyLen, m_responseBufferLen - copyLen);
    data += copyLen;
    m_responseBufferLen -= copyLen;
    for (int dataRestLen = dataLen - copyLen; dataRestLen > 0;)
    {
        int readed = m_tcpSock.recv(data, dataRestLen);
        if (readed <= 0)
            return readed;
        dataRestLen -= readed;
        data += readed;
    }
    demuxedData[channelNumber]->finishWriting(dataLen);
    return dataLen;
}

// demux text data only
bool RTPSession::readTextResponce(QByteArray& response)
{
    bool readMoreData = m_responseBufferLen == 0;
    for (int i = 0; i < 40 && m_tcpSock.isConnected(); ++i)
    {
        if (readMoreData) {
            int readed = m_tcpSock.recv(m_responseBuffer+m_responseBufferLen, qMin(1024, RTSP_BUFFER_LEN - m_responseBufferLen));
            if (readed <= 0)
                return readed;
            m_responseBufferLen += readed;
        }
        if (m_responseBuffer[0] == '$') {
            // binary data
            quint8 tmpData[1024*64];
            readBinaryResponce(tmpData, sizeof(tmpData)); // skip binary data
            QnSleep::msleep(1);
        }
        else {
            // text data
            int msgLen = QnTCPConnectionProcessor::isFullMessage(QByteArray::fromRawData((const char*)m_responseBuffer, m_responseBufferLen));
            if (msgLen > 0)
            {
                response = QByteArray((const char*) m_responseBuffer, msgLen);
                memmove(m_responseBuffer, m_responseBuffer + msgLen, m_responseBufferLen - msgLen);
                m_responseBufferLen -= msgLen;
                return true;
            }
        }
        readMoreData = true;
        if (m_responseBufferLen == RTSP_BUFFER_LEN)
            return false;
    }
    return false;
}

/*
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
                QString tmp = extractRTSPParam(QLatin1String(textResponse), QLatin1String("Range:"));
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
                Q_ASSERT((m_responseBuffer+m_responseBufferLen) - curPtr >= 0);
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
        //qWarning() << "Can't find RTSP text response because of buffer is full! Drop some media data and find againg";
        quint8 tmpBuffer[1024*64];
        if (readBinaryResponce(tmpBuffer, sizeof(tmpBuffer)) == -1)
            break;
    }
    qWarning() << "RTSP Text response not found! RTSP command is failed!";
    return false;
}

*/

QString RTPSession::extractRTSPParam(const QString& buffer, const QString& paramName)
{
    int pos = buffer.indexOf(paramName);
    if (pos > 0)
    {
        QString rez;
        bool first = true;
        for (int i = pos + paramName.size() + 1; i < buffer.size(); i++)
        {
            if (buffer[i] == QLatin1Char(' ') && first)
                continue;
            else if (buffer[i] == QLatin1Char('\r') || buffer[i] == QLatin1Char('\n'))
                break;
            else {
                rez += buffer[i];
                first = false;
            }
        }
        return rez;
    }
    else
        return QString();
}

void RTPSession::updateTransportHeader(QByteArray& responce)
{
    QString tmp = extractRTSPParam(QLatin1String(responce), QLatin1String("Transport:"));
    if (tmp.size() > 0)
    {
        QStringList tmpList = tmp.split(QLatin1Char(';'));
        for (int i = 0; i < tmpList.size(); ++i)
        {
            if (tmpList[i].startsWith(QLatin1String("port")))
            {
                QStringList tmpParams = tmpList[i].split(QLatin1Char('='));
                if (tmpParams.size() > 1)
                    m_ServerPort = tmpParams[1].toInt();
            }
        }
    }
}

void RTPSession::setTransport(TransportType transport)
{
    m_transport = transport;
}

void RTPSession::setTransport(const QString& transport)
{
    if (transport == QLatin1String("TCP"))
        m_transport = TRANSPORT_TCP;
    else if (transport == QLatin1String("UDP"))
        m_transport = TRANSPORT_UDP;
    else
        m_transport = TRANSPORT_AUTO;
}

QString RTPSession::getTrackFormatByRtpChannelNum(int channelNum)
{
    return getTrackFormat(channelNum / SDP_TRACK_STEP);
}

QString RTPSession::getTrackFormat(int trackNum) const
{
    // client setup all track numbers consequentially, so we can use track num as direct vector index. Expect medata track with fixed num and always last record
    if (trackNum < m_sdpTracks.size())
        return m_sdpTracks[trackNum]->codecName;
    else if (trackNum == METADATA_TRACK_NUM && !m_sdpTracks.isEmpty())
        return m_sdpTracks.last()->codecName;
    else
        return QString();
}

RTPSession::TrackType RTPSession::getTrackTypeByRtpChannelNum(int channelNum)
{
    TrackType rez = getTrackType(channelNum / SDP_TRACK_STEP);
    if (channelNum % SDP_TRACK_STEP)
        rez = RTPSession::TrackType(int(rez)+1);
    return rez;
}

RTPSession::TrackType RTPSession::getTrackType(int trackNum) const
{
    // client setup all track numbers consequentially, so we can use track num as direct vector index. Expect medata track with fixed num and always last record
    if (trackNum < m_sdpTracks.size())
        return m_sdpTracks[trackNum]->trackType;
    else if (trackNum == METADATA_TRACK_NUM && !m_sdpTracks.isEmpty())
        return m_sdpTracks.last()->trackType;
    else
        return TT_UNKNOWN;
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


void RTPSession::setAudioEnabled(bool value)
{
    m_isAudioEnabled = value;
}

void RTPSession::setProxyAddr(const QString& addr, int port)
{
    m_proxyAddr = addr;
    m_proxyPort = port;
}

QString RTPSession::mediaTypeToStr(TrackType trackType)
{
    if (trackType == TT_AUDIO)
        return QLatin1String("audio");
    else if (trackType == TT_AUDIO_RTCP)
        return QLatin1String("audio-rtcp");
    else if (trackType == TT_VIDEO)
        return QLatin1String("video");
    else if (trackType == TT_VIDEO_RTCP)
        return QLatin1String("video-rtcp");
    else if (trackType == TT_METADATA)
        return QLatin1String("metadata");
    else
        return QLatin1String("TT_UNKNOWN");
}

void RTPSession::setUsePredefinedTracks(int numOfVideoChannel)
{
    m_numOfPredefinedChannels = numOfVideoChannel;
}
