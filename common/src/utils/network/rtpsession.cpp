#include "rtpsession.h"

#ifdef ENABLE_DATA_PROVIDERS

#if defined(Q_OS_WIN)
#  include <winsock2.h>
#endif

#include "rtp_stream_parser.h"

#include <QtCore/QFile>
#include <utils/common/uuid.h>
#include <utils/network/rtsp/rtsp_types.h>

#include "utils/common/log.h"
#include "utils/common/util.h"
#include "utils/common/systemerror.h"
#include "utils/network/http/httptypes.h"
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

static const int TCP_CONNECT_TIMEOUT = 1000 * 5;
static const int SDP_TRACK_STEP = 2;
static const int METADATA_TRACK_NUM = 7;
static const char USER_AGENT_STR[] = "User-Agent: Network Optix\r\n";
//static const int TIME_RESYNC_THRESHOLD = 15;
//static const int TIME_FUTURE_THRESHOLD = 4;
//static const double TIME_RESYNC_THRESHOLD2 = 30;
static const double TIME_RESYNC_THRESHOLD = 10.0; // at seconds
static const double LOCAL_TIME_RESYNC_THRESHOLD = 500; // at ms
static const int DRIFT_STATS_WINDOW_SIZE = 1000;
static const QString DEFAULT_REALM(lit("NetworkOptix"));


QByteArray RTPSession::m_guid;
QMutex RTPSession::m_guidMutex;

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
    m_rtcpSocket(0),
    ssrc(0),
    m_rtpTrackNum(0)
{
    m_tcpMode = useTCP;
    if (!m_tcpMode) 
    {
        m_mediaSocket = SocketFactory::createDatagramSocket();
        m_mediaSocket->bind( SocketAddress( HostAddress::anyHost, 0 ) );
        m_mediaSocket->setRecvTimeout(500);

        m_rtcpSocket = SocketFactory::createDatagramSocket();
        m_rtcpSocket->bind( SocketAddress( HostAddress::anyHost, 0 ) );
        m_rtcpSocket->setRecvTimeout(500);
    }
}

RTPIODevice::~RTPIODevice()
{
    delete m_mediaSocket;
    delete m_rtcpSocket;
}

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

AbstractCommunicatingSocket* RTPIODevice::getMediaSocket()
{ 
    if (m_tcpMode) 
        return m_owner->m_tcpSock.get();
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
    while( m_rtcpSocket->hasData() )
    {
        QString lastReceivedAddr;
        unsigned short lastReceivedPort = 0;
        int readed = m_rtcpSocket->recvFrom(rtcpBuffer, sizeof(rtcpBuffer), lastReceivedAddr, lastReceivedPort);
        if (readed > 0)
        {
            if (!m_rtcpSocket->isConnected())
            {
                if (!m_rtcpSocket->setDestAddr(lastReceivedAddr, lastReceivedPort))
                {
                    qWarning() << "RTPIODevice::processRtcpData(): setDestAddr() failed: " << SystemError::getLastOSErrorText();
                }
            }
            bool gotValue = false;
            RtspStatistic stats = m_owner->parseServerRTCPReport(rtcpBuffer, readed, &gotValue);
            if (gotValue)
                m_statistic = stats;
            int outBufSize = m_owner->buildClientRTCPReport(sendBuffer, MAX_RTCP_PACKET_SIZE);
            if (outBufSize > 0)
                m_rtcpSocket->send(sendBuffer, outBufSize);
        }
    }
}

void RTPSession::SDPTrackInfo::setSSRC(quint32 value)
{
    ioDevice->setSSRC(value);
}

quint32 RTPSession::SDPTrackInfo::getSSRC() const
{
    return ioDevice->getSSRC();
}

// ================================================== QnRtspTimeHelper ==========================================

QMutex QnRtspTimeHelper::m_camClockMutex;
//!map<resID, <CamSyncInfo, refcount> >
QMap<QString, QPair<QSharedPointer<QnRtspTimeHelper::CamSyncInfo>, int> > QnRtspTimeHelper::m_camClock;


QnRtspTimeHelper::QnRtspTimeHelper(const QString& resId)
:
    m_lastTime(AV_NOPTS_VALUE),
    m_localStartTime(0),
    m_rtcpReportTimeDiff(INT_MAX),
    m_resId(resId)
{
    {
        QMutexLocker lock(&m_camClockMutex);

        QPair<QSharedPointer<QnRtspTimeHelper::CamSyncInfo>, int>& val = m_camClock[resId];
        if( !val.first )
            val.first = QSharedPointer<CamSyncInfo>(new CamSyncInfo());
        m_cameraClockToLocalDiff = val.first;
        ++val.second;   //need ref count, since QSharedPointer does not provide access to its refcount
    }

    m_localStartTime = qnSyncTime->currentMSecsSinceEpoch();
    m_timer.restart();
    m_lastWarnTime = 0;
}

QnRtspTimeHelper::~QnRtspTimeHelper()
{
    {
        QMutexLocker lock(&m_camClockMutex);

        QMap<QString, QPair<QSharedPointer<QnRtspTimeHelper::CamSyncInfo>, int> >::iterator it = m_camClock.find( m_resId );
        if( it != m_camClock.end() )
            if( (--it.value().second) == 0 )
                m_camClock.erase( it );
    }
}

double QnRtspTimeHelper::cameraTimeToLocalTime(double cameraTime)
{
    double localtime = qnSyncTime->currentMSecsSinceEpoch()/1000.0;
    
    QMutexLocker lock(&m_cameraClockToLocalDiff->mutex);
    if (m_cameraClockToLocalDiff->timeDiff == INT_MAX)  
        m_cameraClockToLocalDiff->timeDiff = localtime - cameraTime;
    double result = cameraTime + m_cameraClockToLocalDiff->timeDiff;
    

    qint64 currentDrift = (localtime - result)*1000;
    m_cameraClockToLocalDiff->driftSum += currentDrift;
    m_cameraClockToLocalDiff->driftStats.push(currentDrift);
    if (m_cameraClockToLocalDiff->driftStats.size() > DRIFT_STATS_WINDOW_SIZE) {
        qint64 frontVal;
        m_cameraClockToLocalDiff->driftStats.pop(frontVal);
        m_cameraClockToLocalDiff->driftSum -= frontVal;
    }

    //qDebug() << "drift = " << (int) currentDrift;
    if (m_cameraClockToLocalDiff->driftStats.size() >= DRIFT_STATS_WINDOW_SIZE/4)
    {
        double avgDrift = m_cameraClockToLocalDiff->driftSum / (double) m_cameraClockToLocalDiff->driftStats.size();
        //qDebug() << "avgDrift = " << (int) (avgDrift*1000) << "sz=" << m_cameraClockToLocalDiff->driftStats.size();
        return result + avgDrift/1000.0;
    }
    else {
        return result;
    }
}

void QnRtspTimeHelper::reset()
{
    QMutexLocker lock(&m_cameraClockToLocalDiff->mutex);
    
    m_cameraClockToLocalDiff->timeDiff = INT_MAX;
    m_cameraClockToLocalDiff->driftStats.clear();
    m_cameraClockToLocalDiff->driftSum = 0;
}

bool QnRtspTimeHelper::isLocalTimeChanged()
{
    qint64 elapsed;
    int tryCount = 0;
    qint64 ct;
    do {
        elapsed = m_timer.elapsed();
        ct = qnSyncTime->currentMSecsSinceEpoch();
    } while (m_timer.elapsed() != elapsed && ++tryCount < 3);

    qint64 expectedLocalTime = elapsed + m_localStartTime;
    bool timeChanged = qAbs(expectedLocalTime - ct) > LOCAL_TIME_RESYNC_THRESHOLD;
    if (timeChanged || elapsed > 3600) {
        m_localStartTime = qnSyncTime->currentMSecsSinceEpoch();
        m_timer.restart();
    }
    return timeChanged;
}


bool QnRtspTimeHelper::isCameraTimeChanged(const RtspStatistic& statistics)
{
    double diff = statistics.localtime - statistics.nptTime;
    if (m_rtcpReportTimeDiff == INT_MAX)
        m_rtcpReportTimeDiff = diff;
    bool rez = qAbs(diff - m_rtcpReportTimeDiff) > TIME_RESYNC_THRESHOLD;
    if (rez)
        m_rtcpReportTimeDiff = INT_MAX;
    return rez;
}

#ifdef DEBUG_TIMINGS
void QnRtspTimeHelper::printTime(double jitter)
{
    if (m_statsTimer.elapsed() < 1000)
    {
        m_minJitter = qMin(m_minJitter, jitter);
        m_maxJitter = qMax(m_maxJitter, jitter);
        m_jitterSum += jitter;
        m_jitPackets++;
    }
    else {
        if (m_jitPackets > 0) {
            QString message(QLatin1String("camera %1. minJit=%2 ms. maxJit=%3 ms. avgJit=%4 ms"));
            message = message.arg(m_resId).arg(int(m_minJitter*1000+0.5)).arg(int(m_maxJitter*1000+0.5)).arg(int(m_jitterSum/m_jitPackets*1000+0.5));
            NX_LOG(message, cl_logINFO);
        }
        m_statsTimer.restart();
        m_minJitter = INT_MAX;
        m_maxJitter = 0;
        m_jitterSum = 0;
        m_jitPackets = 0;
    }
}
#endif

qint64 QnRtspTimeHelper::getUsecTime(quint32 rtpTime, const RtspStatistic& statistics, int frequency, bool recursiveAllowed)
{
    if (statistics.isEmpty())
        return qnSyncTime->currentMSecsSinceEpoch() * 1000;
    else {
        int rtpTimeDiff = rtpTime - statistics.timestamp;
        double resultInSecs = cameraTimeToLocalTime(statistics.nptTime + rtpTimeDiff / double(frequency));
        double localTimeInSecs = qnSyncTime->currentMSecsSinceEpoch()/1000.0;
        // If data is delayed for some reason > than jitter, but not lost, some next data can have timing less then previous data (after reinit).
        // Such data can not be recorded to archive. I ofter got that situation if server under debug
        // So, I've increased jitter threshold just in case (very slow mediaServer work e.t.c)
        // In any way, valid threshold behaviour if camera time is changed.
        //if (qAbs(localTimeInSecs - resultInSecs) < 15 || !recursiveAllowed) 

        //bool cameraTimeChanged = m_lastResultInSec != 0 && qAbs(resultInSecs - m_lastResultInSec) > TIME_RESYNC_THRESHOLD;
        //bool gotInvalidTime = resultInSecs - localTimeInSecs > TIME_FUTURE_THRESHOLD;
        //if ((cameraTimeChanged || isLocalTimeChanged() || gotInvalidTime) && recursiveAllowed)

        //if (localTimeInSecs - resultInSecs > TIME_RESYNC_THRESHOLD2)
        //    m_lastTime = 0; // time goes to the past

        //qWarning() << "RTSP time drift reached" << localTimeInSecs - resultInSecs;
        //if (qAbs(localTimeInSecs - resultInSecs) > TIME_RESYNC_THRESHOLD && recursiveAllowed) 

        double jitter = qAbs(resultInSecs - localTimeInSecs);
        bool gotInvalidTime = jitter > TIME_RESYNC_THRESHOLD;
        bool camTimeChanged = isCameraTimeChanged(statistics);
        bool localTimeChanged = isLocalTimeChanged();

#ifdef DEBUG_TIMINGS
        printTime(jitter);
#endif

        if ((camTimeChanged || localTimeChanged || gotInvalidTime) && recursiveAllowed)
        {
            qint64 currentUsecTime = getUsecTimer();
            if (currentUsecTime - m_lastWarnTime > 2000 * 1000ll)
            {
                if (camTimeChanged) {
                    NX_LOG(QString(lit("Camera time has been changed or receiving latency > 10 seconds. Resync time for camera %1")).arg(m_resId), cl_logWARNING);
                }
                else if (localTimeChanged) {
                    NX_LOG(QString(lit("Local time has been changed. Resync time for camera %1")).arg(m_resId), cl_logWARNING);
                }
                else {
                    NX_LOG(QString(lit("RTSP time drift reached %1 seconds. Resync time for camera %2")).arg(localTimeInSecs - resultInSecs).arg(m_resId), cl_logWARNING);
                }
                m_lastWarnTime = currentUsecTime;
            }
            reset();
            if (localTimeChanged)
                m_lastTime = AV_NOPTS_VALUE;
            return getUsecTime(rtpTime, statistics, frequency, false);
        }
        else {
            if (gotInvalidTime)
                resultInSecs = localTimeInSecs;
            //m_lastResultInSec = resultInSecs;
            qint64 rez = resultInSecs * 1000000ll;
            // check for negative time if camera timings is inaccurate
            if (m_lastTime != (qint64)AV_NOPTS_VALUE && rez <= m_lastTime)
                rez = m_lastTime + MIN_FRAME_DURATION;
            m_lastTime = rez;

            //qDebug() << "rtspTime=" << QDateTime::fromMSecsSinceEpoch(rez/1000).toString(QLatin1String("hh:mm:ss.zzz")) << "localtime=" << QDateTime::currentDateTime().toString(QLatin1String("hh:mm:ss.zzz"));

            return rez;
        }
    }
}


static const size_t ADDITIONAL_READ_BUFFER_CAPACITY = 64*1024;

// ================================================== RTPSession ==========================================
RTPSession::RTPSession():
    m_csec(2),
    //m_rtpIo(*this),
    m_transport(TRANSPORT_UDP),
    m_selectedAudioChannel(0),
    m_startTime(AV_NOPTS_VALUE),
    m_openedTime(AV_NOPTS_VALUE),
    m_endTime(AV_NOPTS_VALUE),
    m_scale(1.0),
    m_tcpTimeout(10 * 1000),
    m_proxyPort(0),
    m_responseCode(CODE_OK),
    m_isAudioEnabled(true),
    m_useDigestAuth(false),
    m_numOfPredefinedChannels(0),
    m_TimeOut(0),
    m_additionalReadBuffer( nullptr ),
    m_additionalReadBufferPos( 0 ),
    m_additionalReadBufferSize( 0 )
{
    m_responseBuffer = new quint8[RTSP_BUFFER_LEN];
    m_responseBufferLen = 0;

    m_tcpSock.reset( SocketFactory::createStreamSocket() );

    m_additionalReadBuffer = new char[ADDITIONAL_READ_BUFFER_CAPACITY];

    // todo: debug only remove me
}

RTPSession::~RTPSession()
{
    stop();
    delete [] m_responseBuffer;

    delete[] m_additionalReadBuffer;
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

bool trackNumLess(const QSharedPointer<RTPSession::SDPTrackInfo>& track1, const QSharedPointer<RTPSession::SDPTrackInfo>& track2)
{
    return track1->trackNum < track2->trackNum;
}

void RTPSession::updateTrackNum()
{
    int videoNum = 0;
    int audioNum = 0;
    int metadataNum = 0;
    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        if (m_sdpTracks[i]->trackType == TT_VIDEO)
            videoNum++;
        else if (m_sdpTracks[i]->trackType == TT_AUDIO)
            audioNum++;
        else if (m_sdpTracks[i]->trackType == TT_METADATA)
            metadataNum++;
    }

    int curVideo = 0;
    int curAudio = 0;
    int curMetadata = 0;

    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        if (m_sdpTracks[i]->trackType == TT_VIDEO)
            m_sdpTracks[i]->trackNum = curVideo++;
        else if (m_sdpTracks[i]->trackType == TT_AUDIO)
            m_sdpTracks[i]->trackNum = videoNum + curAudio++;
        else if (m_sdpTracks[i]->trackType == TT_METADATA) {
            if (m_sdpTracks[i]->codecName == QLatin1String("ffmpeg-metadata"))
                m_sdpTracks[i]->trackNum = METADATA_TRACK_NUM; // use fixed track num for our proprietary format
            else
                m_sdpTracks[i]->trackNum = videoNum + audioNum + curMetadata++;
        }
        else
            m_sdpTracks[i]->trackNum = videoNum + audioNum + metadataNum; // unknown track
    }
    qSort(m_sdpTracks.begin(), m_sdpTracks.end(), trackNumLess);
}


void RTPSession::parseSDP()
{
    QList<QByteArray> lines = m_sdp.split('\n');

    int mapNum = -1;
    QString codecName;
    QString codecType;
    QString setupURL;

    for(QByteArray line: lines)
    {
        line = line.trimmed();
        QByteArray lineLower = line.toLower();
        if (lineLower.startsWith("m="))
        {
            if (mapNum >= 0) {
                if (codecName.isEmpty())
                    codecName = findCodecById(mapNum);
                QSharedPointer<SDPTrackInfo> sdpTrack(new SDPTrackInfo(codecName, codecType, setupURL, mapNum, 0, this, m_transport == TRANSPORT_TCP));
                m_sdpTracks << sdpTrack;
                setupURL.clear();
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
        //if (codecName == QLatin1String("ffmpeg-metadata"))
        //    trackNumber = METADATA_TRACK_NUM;
        QSharedPointer<SDPTrackInfo> sdpTrack(new SDPTrackInfo(codecName, codecType, setupURL, mapNum, 0, this, m_transport == TRANSPORT_TCP));
        m_sdpTracks << sdpTrack;
    }
    updateTrackNum();
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
    if (firstLineEnd >= 0)
    {
        nx_http::StatusLine statusLine;
        statusLine.parse( nx_http::ConstBufferRefType(response, 0, firstLineEnd) );
        m_responseCode = statusLine.statusCode;
        m_reasonPhrase = QLatin1String(statusLine.reasonPhrase);
    }
}

CameraDiagnostics::Result RTPSession::open(const QString& url, qint64 startTime)
{
    if ((quint64)startTime != AV_NOPTS_VALUE)
        m_openedTime = startTime;
    m_SessionId.clear();
    m_responseCode = CODE_OK;
    mUrl = url;
    m_contentBase = mUrl.toString();
    m_responseBufferLen = 0;
    m_rtpToTrack.clear();
    m_rtspAuthCtx.clear();



    //unsigned int port = DEFAULT_RTP_PORT;

    if (m_tcpSock->isClosed())
    {
        m_tcpSock->reopen();
        m_additionalReadBufferSize = 0;
    }

    m_tcpSock->setRecvTimeout(TCP_CONNECT_TIMEOUT);

    QString targetAddress;
    int destinationPort = 0;
    if( m_proxyPort == 0 )
    {
        targetAddress = mUrl.host();
        destinationPort = mUrl.port(DEFAULT_RTP_PORT);
    }
    else
    {
        targetAddress = m_proxyAddr;
        destinationPort = m_proxyPort;
    }
    if( !m_tcpSock->connect(targetAddress, destinationPort) )
        return CameraDiagnostics::CannotOpenCameraMediaPortResult(url, destinationPort);

    m_tcpSock->setNoDelay(true);

    m_tcpSock->setRecvTimeout(m_tcpTimeout);
    m_tcpSock->setSendTimeout(m_tcpTimeout);

    if (m_numOfPredefinedChannels) {
        usePredefinedTracks();
        return CameraDiagnostics::NoErrorResult();
    }

    QByteArray response;
    if( !sendRequestAndReceiveResponse( createDescribeRequest(), response ) )
    {
        stop();
        return CameraDiagnostics::ConnectionClosedUnexpectedlyResult(url, destinationPort);
    }

    QString tmp = extractRTSPParam(QLatin1String(response), QLatin1String("Range:"));
    if (!tmp.isEmpty())
        parseRangeHeader(tmp);

    tmp = extractRTSPParam(QLatin1String(response), QLatin1String("Content-Base:"));
    if (!tmp.isEmpty())
        m_contentBase = tmp;


    CameraDiagnostics::Result result = CameraDiagnostics::NoErrorResult();
    updateResponseStatus(response);
    switch( m_responseCode )
    {
        case CL_HTTP_SUCCESS:
            break;
        case CL_HTTP_AUTH_REQUIRED:
        case nx_http::StatusCode::proxyAuthenticationRequired:
            stop();
            return CameraDiagnostics::NotAuthorisedResult( url );
        default:
            stop();
            return CameraDiagnostics::RequestFailedResult( lit("DESCRIBE %1").arg(url), m_reasonPhrase );
    }

    int sdp_index = response.indexOf(QLatin1String("\r\n\r\n"));

    if (sdp_index  < 0 || sdp_index+4 >= response.size()) {
        stop();
        return CameraDiagnostics::NoMediaTrackResult( url );
    }

    m_sdp = response.mid(sdp_index+4);
    parseSDP();

    if (m_sdpTracks.size()<=0) {
        stop();
        result = CameraDiagnostics::NoMediaTrackResult( url );
    }

    return result;
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
    QMutexLocker lock(&m_sockMutex);
    m_tcpSock->close();
    return true;
}

bool RTPSession::isOpened() const
{
    return m_tcpSock->isConnected();
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

QByteArray RTPSession::calcDefaultNonce() const
{
    return QByteArray::number(qnSyncTime->currentUSecsSinceEpoch() , 16);
}

#if 1
void RTPSession::addAuth( nx_http::Request* const request )
{
    qnAuthHelper->authenticate(
        m_auth,
        request,
        &m_rtspAuthCtx );   //ignoring result
}
#else
void RTPSession::addAuth(QByteArray& request)
{
    if (m_auth.isNull())
        return;
    if (m_useDigestAuth)
    {
        QByteArray firstLine = request.left(request.indexOf('\n'));
        QList<QByteArray> methodAndUri = firstLine.split(' ');
        if (methodAndUri.size() >= 2) {
            QString uri = QLatin1String(methodAndUri[1]);
            if (uri.startsWith(lit("rtsp://")))
                uri = QUrl(uri).path();
            request.append( CLSimpleHTTPClient::digestAccess(
                m_auth, m_realm, m_nonce, QLatin1String(methodAndUri[0]),
                uri, m_responseCode == nx_http::StatusCode::proxyAuthenticationRequired ));
        }
    }
    else {
        request.append(CLSimpleHTTPClient::basicAuth(m_auth));
        request.append(QLatin1String("\r\n"));
    }
}   
#endif

nx_http::Request RTPSession::createDescribeRequest()
{
    m_sdpTracks.clear();

    nx_http::Request request;
    request.requestLine.method = "DESCRIBE";
    request.requestLine.url = mUrl;
    request.requestLine.version = nx_rtsp::rtsp_1_0;
    request.headers.insert( nx_http::HttpHeader( "CSeq", QByteArray::number(m_csec++) ) );
    request.headers.insert( nx_http::parseHeader(nx::Buffer(USER_AGENT_STR)) );
    request.headers.insert( nx_http::HttpHeader( "Accept", "application/sdp" ) );
    if( (quint64)m_openedTime != AV_NOPTS_VALUE )
        addRangeHeader( &request, m_openedTime, AV_NOPTS_VALUE );
    return request;
}

bool RTPSession::sendRequestInternal(nx_http::Request&& request)
{
    addAuth(&request);
    addAdditionAttrs(&request);
    QByteArray requestBuf;
    request.serialize( &requestBuf );
    return m_tcpSock->send(requestBuf.constData(), requestBuf.size()) > 0;
}

bool RTPSession::sendDescribe()
{
    return sendRequestInternal(createDescribeRequest());
}

bool RTPSession::sendOptions()
{
    nx_http::Request request;
    request.requestLine.method = "OPTIONS";
    request.requestLine.url = mUrl;
    request.requestLine.version = nx_rtsp::rtsp_1_0;
    request.headers.insert( nx_http::HttpHeader( "CSeq", QByteArray::number(m_csec++) ) );
    request.headers.insert( nx_http::parseHeader(nx::Buffer(USER_AGENT_STR)) );
    return sendRequestInternal(std::move(request));
}

int RTPSession::getTrackCount(TrackType trackType) const
{
    int result = 0;
    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        if (m_sdpTracks[i]->trackType == trackType)
            ++result;
    }
    return result;
}

QList<QByteArray> RTPSession::getSdpByType(TrackType trackType) const
{
    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        if (m_sdpTracks[i]->trackType == trackType)
            return getSdpByTrackNum(i);
    }
    return QList<QByteArray>();
}

QList<QByteArray> RTPSession::getSdpByTrackNum(int trackNum) const
{
    QList<QByteArray> rez;
    QList<QByteArray> tmp = m_sdp.split('\n');
    
    int mapNum = -1;
    for (int i = 0; i < m_sdpTracks.size(); ++i)
    {
        if (trackNum == m_sdpTracks[i]->trackNum)
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

void RTPSession::registerRTPChannel(int rtpNum, QSharedPointer<SDPTrackInfo> trackInfo)
{
    while (m_rtpToTrack.size() <= rtpNum)
        m_rtpToTrack << QSharedPointer<SDPTrackInfo>();
    m_rtpToTrack[rtpNum] = trackInfo;
};

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

#if 0
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
        request += USER_AGENT_STR;
        request += "Transport: RTP/AVP/";
        request += m_prefferedTransport == TRANSPORT_UDP ? "UDP" : "TCP";
        request += ";unicast;";

        if (m_prefferedTransport == TRANSPORT_UDP)
        {
            request += "client_port=";
            request += QString::number(trackInfo->ioDevice->getMediaSocket()->getLocalAddress().port);
            request += '-';
            request += QString::number(trackInfo->ioDevice->getRtcpSocket()->getLocalAddress().port);
        }
        else
        {
        	int rtpNum = trackInfo->trackNum*SDP_TRACK_STEP;
            trackInfo->interleaved = QPair<int,int>(rtpNum, rtpNum+1);
            request += QLatin1String("interleaved=") + QString::number(trackInfo->interleaved.first) + QLatin1Char('-') + QString::number(trackInfo->interleaved.second);
        }
        request += "\r\n";

        if (!m_SessionId.isEmpty()) {
            request += "Session: ";
            request += m_SessionId;
            request += "\r\n";
        }


        request += "\r\n";

        //qDebug() << request;
#else

        nx_http::Request request;
        request.requestLine.method = "SETUP";
        if( trackInfo->setupURL.startsWith(QLatin1String("rtsp://")) )
        {
            // full track url in a prefix
            request.requestLine.url = trackInfo->setupURL;
        }   
        else
        {
            request.requestLine.url = mUrl;
            // SETUP postfix should be writen after url query params. It's invalid url, but it's required according to RTSP standard
            request.requestLine.urlPostfix = QString(lit("/")) + trackInfo->setupURL;
        }
        request.requestLine.version = nx_rtsp::rtsp_1_0;
        request.headers.insert( nx_http::HttpHeader( "CSeq", QByteArray::number(m_csec++) ) );
        request.headers.insert( nx_http::parseHeader(nx::Buffer(USER_AGENT_STR)) );

        {   //generating transport header
            nx_http::StringType transportStr = "RTP/AVP/";
            transportStr += m_prefferedTransport == TRANSPORT_UDP ? "UDP" : "TCP";
            transportStr += ";unicast;";

            if (m_prefferedTransport == TRANSPORT_UDP)
            {
                transportStr += "client_port=";
                transportStr += QString::number(trackInfo->ioDevice->getMediaSocket()->getLocalAddress().port);
                transportStr += '-';
                transportStr += QString::number(trackInfo->ioDevice->getRtcpSocket()->getLocalAddress().port);
            }
            else
            {
        	    int rtpNum = trackInfo->trackNum*SDP_TRACK_STEP;
                trackInfo->interleaved = QPair<int,int>(rtpNum, rtpNum+1);
                transportStr += QLatin1String("interleaved=") + QString::number(trackInfo->interleaved.first) + QLatin1Char('-') + QString::number(trackInfo->interleaved.second);
            }
            request.headers.insert( nx_http::HttpHeader( "Transport", transportStr ) );
        }

        if( !m_SessionId.isEmpty() )
            request.headers.insert( nx_http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
#endif

        QByteArray responce;
        if( !sendRequestAndReceiveResponse( std::move(request), responce ) )
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

        QString sessionParam = extractRTSPParam(QLatin1String(responce), QLatin1String("Session:"));
        if (sessionParam.size() > 0)
        {
            QStringList tmpList = sessionParam.split(QLatin1Char(';'));
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

        QString transportParam = extractRTSPParam(QLatin1String(responce), QLatin1String("Transport:"));
        if (transportParam.size() > 0)
        {
            QStringList tmpList = transportParam.split(QLatin1Char(';'));
            for (int k = 0; k < tmpList.size(); ++k)
            {
                tmpList[k] = tmpList[k].trimmed().toLower();
                if (tmpList[k].startsWith(QLatin1String("ssrc")))
                {
                    QStringList tmpParams = tmpList[k].split(QLatin1Char('='));
                    if (tmpParams.size() > 1) {
                        bool ok;
                        m_sdpTracks[i]->setSSRC(tmpParams[1].toInt(&ok, 16));
                    }
                }
                else if (tmpList[k].startsWith(QLatin1String("interleaved"))) {
                    QStringList tmpParams = tmpList[k].split(QLatin1Char('='));
                    if (tmpParams.size() > 1) {
                        tmpParams = tmpParams[1].split(QLatin1String("-"));
                        if (tmpParams.size() == 2) {
                            trackInfo->interleaved = QPair<int,int>(tmpParams[0].toInt(), tmpParams[1].toInt());
                            registerRTPChannel(trackInfo->interleaved.first, trackInfo);
                        }
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

void RTPSession::addAdditionAttrs( nx_http::Request* const request )
{
    for (QMap<QByteArray, QByteArray>::const_iterator i = m_additionAttrs.begin(); i != m_additionAttrs.end(); ++i)
        request->headers.insert( nx_http::HttpHeader( i.key(), i.value() ) );
}

bool RTPSession::sendSetParameter( const QByteArray& paramName, const QByteArray& paramValue )
{
    nx_http::Request request;

    request.messageBody.append(paramName);
    request.messageBody.append(": ");
    request.messageBody.append(paramValue);
    request.messageBody.append("\r\n");

    request.requestLine.method = "SET_PARAMETER";
    request.requestLine.url = mUrl;
    request.requestLine.version = nx_rtsp::rtsp_1_0;
    request.headers.insert( nx_http::HttpHeader( "CSeq", QByteArray::number(m_csec++) ) );
    request.headers.insert( nx_http::parseHeader(nx::Buffer(USER_AGENT_STR)) );
    request.headers.insert( nx_http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    request.headers.insert( nx_http::HttpHeader( "Content-Length", QByteArray::number(request.messageBody.size()) ) );
    return sendRequestInternal(std::move(request));
}

void RTPSession::addRangeHeader( nx_http::Request* const request, qint64 startPos, qint64 endPos )
{
    if (startPos != qint64(AV_NOPTS_VALUE))
    {
        nx_http::StringType rangeVal;
        if (startPos != DATETIME_NOW)
            rangeVal += "npt=" + nx_http::StringType::number(startPos);
        else
            rangeVal += "npt=now";
        rangeVal += '-';
        if (endPos != qint64(AV_NOPTS_VALUE))
        {
            if (endPos != DATETIME_NOW)
                rangeVal += nx_http::StringType::number(endPos);
            else
                rangeVal += "now";
        }
        request->headers.insert( nx_http::HttpHeader( "Range", rangeVal ) );
    }
}

QByteArray RTPSession::getGuid()
{
    QMutexLocker lock(&m_guidMutex);
    if (m_guid.isEmpty())
        m_guid = QnUuid::createUuid().toString().toUtf8();
    return m_guid;
}

nx_http::Request RTPSession::createPlayRequest( qint64 startPos, qint64 endPos )
{
    nx_http::Request request;
    request.requestLine.method = "PLAY";
    request.requestLine.url = m_contentBase;
    request.requestLine.version = nx_rtsp::rtsp_1_0;
    request.headers.insert( nx_http::HttpHeader( "CSeq", QByteArray::number(m_csec++) ) );
    request.headers.insert( nx_http::parseHeader(nx::Buffer(USER_AGENT_STR)) );
    request.headers.insert( nx_http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    addRangeHeader( &request, startPos, endPos );
    request.headers.insert( nx_http::HttpHeader( "Scale", QByteArray::number(m_scale) ) );
    if( m_numOfPredefinedChannels )
    {
        request.headers.insert( nx_http::HttpHeader( "x-play-now", "true" ) );
        request.headers.insert( nx_http::HttpHeader( "x-guid", getGuid() ) );
    }
    return request;
}

bool RTPSession::sendPlayInternal(qint64 startPos, qint64 endPos)
{
    return sendRequestInternal(createPlayRequest( startPos, endPos ));
}

bool RTPSession::sendPlay(qint64 startPos, qint64 endPos, double scale)
{
    QByteArray response;

    m_scale = scale;

    nx_http::Request request = createPlayRequest( startPos, endPos );
    if( !sendRequestAndReceiveResponse( std::move(request), response ) )
    {
        stop();
        return false;
    }

    while (1)
    {
        QString cseq = extractRTSPParam(QLatin1String(response), QLatin1String("CSeq:"));
        if (cseq.toInt() == (int)m_csec-1)
            break;
        else if (!readTextResponce(response))
            return false; // try next response
    }    

    QString tmp = extractRTSPParam(QLatin1String(response), QLatin1String("Range:"));
    if (!tmp.isEmpty())
        parseRangeHeader(tmp);

    tmp = extractRTSPParam(QLatin1String(response), QLatin1String("x-video-layout:"));
    if (!tmp.isEmpty())
        m_videoLayout = tmp; // refactor here: add all attributes to list

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
    nx_http::Request request;
    request.requestLine.method = "PAUSE";
    request.requestLine.url = mUrl;
    request.requestLine.version = nx_rtsp::rtsp_1_0;
    request.headers.insert( nx_http::HttpHeader( "CSeq", QByteArray::number(m_csec++) ) );
    request.headers.insert( nx_http::parseHeader(nx::Buffer(USER_AGENT_STR)) );
    request.headers.insert( nx_http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    return sendRequestInternal(std::move(request));
}

bool RTPSession::sendTeardown()
{
    nx_http::Request request;
    request.requestLine.method = "TEARDOWN";
    request.requestLine.url = mUrl;
    request.requestLine.version = nx_rtsp::rtsp_1_0;
    request.headers.insert( nx_http::HttpHeader( "CSeq", QByteArray::number(m_csec++) ) );
    request.headers.insert( nx_http::parseHeader(nx::Buffer(USER_AGENT_STR)) );
    request.headers.insert( nx_http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    return sendRequestInternal(std::move(request));
}

static const int RTCP_SENDER_REPORT = 200;
static const int RTCP_RECEIVER_REPORT = 201;
static const int RTCP_SOURCE_DESCRIPTION = 202;

RtspStatistic RTPSession::parseServerRTCPReport(quint8* srcBuffer, int srcBufferSize, bool* gotStatistics)
{
    static quint32 rtspTimeDiff = QDateTime::fromString(QLatin1String("1900-01-01"), Qt::ISODate)
        .secsTo(QDateTime::fromString(QLatin1String("1970-01-01"), Qt::ISODate));

    RtspStatistic stats;
    *gotStatistics = false;
    try {
        BitStreamReader reader(srcBuffer, srcBuffer + srcBufferSize);
        reader.skipBits(8); // skip version

        while (reader.getBitsLeft() > 0)
        {
            int messageCode = reader.getBits(8);
            int messageLen = reader.getBits(16);
            Q_UNUSED(messageLen)
            if (messageCode == RTCP_SENDER_REPORT)
            {
                stats.ssrc = reader.getBits(32);
                quint32 intTime = reader.getBits(32);
                quint32 fracTime = reader.getBits(32);
                stats.nptTime = intTime + (double) fracTime / UINT_MAX - rtspTimeDiff;
                stats.localtime = qnSyncTime->currentMSecsSinceEpoch()/1000.0;
                stats.timestamp = reader.getBits(32);
                stats.receivedPackets = reader.getBits(32);
                stats.receivedOctets = reader.getBits(32);
                *gotStatistics = true;
                break;
            }
            else {
                break;
            }
        }
    } catch(...)
    {
        
    }
    return stats;
}


int RTPSession::buildClientRTCPReport(quint8* dstBuffer, int bufferLen)
{
    QByteArray esDescr("netoptix");

    Q_ASSERT(bufferLen >= 20 + esDescr.size());

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
    nx_http::Request request;
    request.requestLine.method = "GET_PARAMETER";
    request.requestLine.url = mUrl;
    request.requestLine.version = nx_rtsp::rtsp_1_0;
    request.headers.insert( nx_http::HttpHeader( "CSeq", QByteArray::number(m_csec++) ) );
    request.headers.insert( nx_http::parseHeader(nx::Buffer(USER_AGENT_STR)) );
    request.headers.insert( nx_http::HttpHeader( "Session", m_SessionId.toLatin1() ) );
    return sendRequestInternal(std::move(request));
}

void RTPSession::sendBynaryResponse(quint8* buffer, int size)
{
    m_tcpSock->send(buffer, size);
}


bool RTPSession::processTextResponseInsideBinData()
{
    // have text response or part of text response.
    int readed = readSocketWithBuffering(m_responseBuffer+m_responseBufferLen, qMin(1024, RTSP_BUFFER_LEN - m_responseBufferLen), true);
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
    while (m_tcpSock->isConnected())
    {
        while (m_responseBufferLen < 4) {
            int readed = readSocketWithBuffering(m_responseBuffer+m_responseBufferLen, 4 - m_responseBufferLen, true);
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
        int readed = readSocketWithBuffering(data, dataRestLen, true);
        if (readed <= 0)
            return readed;
        dataRestLen -= readed;
        data += readed;
    }
    return dataLen;
}

quint8* RTPSession::prepareDemuxedData(std::vector<QnByteArray*>& demuxedData, int channel, int reserve)
{
    if (channel >= 0 && demuxedData.size() <= (size_t)channel)
        demuxedData.resize(channel+1);
    if (demuxedData[channel] == 0)
        demuxedData[channel] = new QnByteArray(16, 32);
    QnByteArray* dataVect = demuxedData[channel];
    //dataVect->resize(dataVect->size() + reserve);
    dataVect->reserve(dataVect->size() + reserve);
    return (quint8*) dataVect->data() + dataVect->size();
}

int RTPSession::readBinaryResponce(std::vector<QnByteArray*>& demuxedData, int& channelNumber)
{
    while (m_tcpSock->isConnected())
    {
        while (m_responseBufferLen < 4) {
            int readed = readSocketWithBuffering(m_responseBuffer+m_responseBufferLen, 4 - m_responseBufferLen, true);
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
        int readed = readSocketWithBuffering(data, dataRestLen, true);
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
    int ignoreDataSize = 0;
    for (int i = 0; i < 1000 && ignoreDataSize < 1024*1024*3 && m_tcpSock->isConnected(); ++i)
    {
        if (readMoreData) {
            int readed = readSocketWithBuffering(m_responseBuffer+m_responseBufferLen, qMin(1024, RTSP_BUFFER_LEN - m_responseBufferLen), true);
            if (readed <= 0)
            {
                if( readed == 0 )
                {
                    NX_LOG( lit("RTSP connection to %1 has been unexpectedly closed").
                        arg(m_tcpSock->getForeignAddress().toString()), cl_logINFO );
                }
                else if (!m_tcpSock->isClosed())
                {
                    NX_LOG( lit("Error reading RTSP response from %1. %2").
                        arg(m_tcpSock->getForeignAddress().toString()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
                }
                return false;	//error occured or connection closed
            }
            m_responseBufferLen += readed;
        }
        if (m_responseBuffer[0] == '$') {
            // binary data
            quint8 tmpData[1024*64];
            int readed = readBinaryResponce(tmpData, sizeof(tmpData)); // skip binary data
            int oldIgnoreDataSize = ignoreDataSize;
            ignoreDataSize += readed;
            if (oldIgnoreDataSize / 64000 != ignoreDataSize/64000)
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
        {
            NX_LOG( lit("RTSP response from %1 has exceeded max response size (%2)").
                arg(m_tcpSock->getForeignAddress().toString()).arg(RTSP_BUFFER_LEN), cl_logINFO );
            return false;
        }
    }
    return false;
}

QString RTPSession::extractRTSPParam(const QString& buffer, const QString& paramName)
{
    int pos = buffer.indexOf(paramName, 0, Qt::CaseInsensitive);
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
    TrackType rez = TT_UNKNOWN;
    int rtpChannelNum = channelNum & ~1;
    if (rtpChannelNum < m_rtpToTrack.size()) {
        QSharedPointer<SDPTrackInfo> track = m_rtpToTrack[rtpChannelNum];
        if (track)
            rez = track->trackType;
    }
    //following code was a camera rtsp bug workaround. Which camera no one can recall. 
        //Commented because it interferes with another bug on TP-LINK shitty camera.
        //So, let's try to be closer to RTSP rfc
    //if (rez == TT_UNKNOWN)
    //    rez = getTrackType(channelNum / SDP_TRACK_STEP);
    if (channelNum % SDP_TRACK_STEP)
        rez = RTPSession::TrackType(int(rez)+1);
    return rez;
}

int RTPSession::getChannelNum(int rtpChannelNum)
{
    rtpChannelNum = rtpChannelNum & ~1;
    if (rtpChannelNum < m_rtpToTrack.size()) {
        QSharedPointer<SDPTrackInfo> track = m_rtpToTrack[rtpChannelNum];
        if (track)
            return track->trackNum;
    }
    return 0;
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

void RTPSession::setAuth(const QAuthenticator& auth, DefaultAuthScheme defaultAuthScheme)
{
    m_auth = auth;
    if (defaultAuthScheme == authDigest) {
        m_useDigestAuth = true;
        m_realm = DEFAULT_REALM;
        m_nonce = QLatin1String(calcDefaultNonce());
    }
}

QAuthenticator RTPSession::getAuth() const
{
    return m_auth;
}


void RTPSession::setAudioEnabled(bool value)
{
    m_isAudioEnabled = value;
}

bool RTPSession::isAudioEnabled() const
{
    return m_isAudioEnabled;
}

void RTPSession::setProxyAddr(const QString& addr, int port)
{
    m_proxyAddr = addr;
    m_proxyPort = port;
}

QString RTPSession::mediaTypeToStr(TrackType trackType)
{
    if (trackType == TT_AUDIO)
        return lit("audio");
    else if (trackType == TT_AUDIO_RTCP)
        return lit("audio-rtcp");
    else if (trackType == TT_VIDEO)
        return lit("video");
    else if (trackType == TT_VIDEO_RTCP)
        return lit("video-rtcp");
    else if (trackType == TT_METADATA)
        return lit("metadata");
    else
        return lit("TT_UNKNOWN");
}

void RTPSession::setUsePredefinedTracks(int numOfVideoChannel)
{
    m_numOfPredefinedChannels = numOfVideoChannel;
}

bool RTPSession::setTCPReadBufferSize(int value)
{
    return m_tcpSock->setRecvBufferSize(value);
}

QString RTPSession::getVideoLayout() const
{
    return m_videoLayout;
}

int RTPSession::readSocketWithBuffering( quint8* buf, size_t bufSize, bool readSome )
{
    const size_t bufSizeBak = bufSize;

    //this method introduced to minimize m_tcpSock->recv calls (on isd edge m_tcpSock->recv call is rather heavy)
    //TODO: #ak remove extra data copying

    for( ;; )
    {
        if( m_additionalReadBufferSize > 0 )
        {
            const size_t bytesToCopy = std::min<int>( m_additionalReadBufferSize, bufSize );
            memcpy( buf, m_additionalReadBuffer + m_additionalReadBufferPos, bytesToCopy );
            m_additionalReadBufferPos += bytesToCopy;
            m_additionalReadBufferSize -= bytesToCopy;
            bufSize -= bytesToCopy;
            buf += bytesToCopy;
        }

        if( readSome && (bufSize < bufSizeBak) )
            return bufSizeBak - bufSize;
        if( bufSize == 0 )
            return bufSizeBak;

        m_additionalReadBufferSize = m_tcpSock->recv( m_additionalReadBuffer, ADDITIONAL_READ_BUFFER_CAPACITY );
        m_additionalReadBufferPos = 0;
        if( m_additionalReadBufferSize <= 0 )
            return bufSize == bufSizeBak
                ? m_additionalReadBufferSize    //if could not read anything returning error
                : bufSizeBak - bufSize;
    }
}

bool RTPSession::sendRequestAndReceiveResponse( nx_http::Request&& request, QByteArray& responseBuf )
{
    int prevStatusCode = nx_http::StatusCode::ok;

    addAuth( &request );
    addAdditionAttrs( &request );

    for( int i = 0; i < 3; ++i )    //needed to avoid infinite loop in case of incorrect server behavour
    {
        QByteArray requestBuf;
        request.serialize( &requestBuf );
        if( m_tcpSock->send(requestBuf.constData(), requestBuf.size()) <= 0 )
            return false;

        if( !readTextResponce(responseBuf) )
            return false;

        nx_http::Response response;
        if( !response.parse( responseBuf ) )
            return false;
        m_responseCode = response.statusLine.statusCode;

        switch( response.statusLine.statusCode )
        {
            case nx_http::StatusCode::unauthorized:
            case nx_http::StatusCode::proxyAuthenticationRequired:
                if( prevStatusCode == response.statusLine.statusCode )
                    return false;   //already tried authentication and have been rejected
                prevStatusCode = response.statusLine.statusCode;
                break;

            default:
                return true;
        }

        if( !qnAuthHelper->authenticate(
                m_auth,
                response,
                &request,
                &m_rtspAuthCtx ) )
            return false;
    }

    return false;
}

RTPSession::TrackMap RTPSession::getTrackInfo() const
{
    return m_sdpTracks;
}

#endif // ENABLE_DATA_PROVIDERS
