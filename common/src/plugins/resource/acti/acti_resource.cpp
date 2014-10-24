#ifdef ENABLE_ACTI

#include "acti_resource.h"

#include <functional>
#include <memory>

#include "api/app_server_connection.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "acti_stream_reader.h"
#include <business/business_event_rule.h>
#include "utils/common/synctime.h"
#include "acti_ptz_controller.h"
#include "rest/server/rest_connection_processor.h"


const QString QnActiResource::MANUFACTURE(lit("ACTI"));
static const int TCP_TIMEOUT = 3000;
static const int DEFAULT_RTSP_PORT = 7070;

static int actiEventPort = 0;

static int DEFAULT_AVAIL_BITRATE_KBPS[] = { 28, 56, 128, 256, 384, 500, 750, 1000, 1200, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000 };

QnActiResource::QnActiResource()
:
    m_rtspPort(DEFAULT_RTSP_PORT),
    m_hasAudio(false),
    m_outputCount(0),
    m_inputCount(0),
    m_inputMonitored(false)
{
    setVendor(lit("ACTI"));

    setAuth(QLatin1String("admin"), QLatin1String("123456"), false);
    for (uint i = 0; i < sizeof(DEFAULT_AVAIL_BITRATE_KBPS)/sizeof(int); ++i)
        m_availBitrate << DEFAULT_AVAIL_BITRATE_KBPS[i];
}

QnActiResource::~QnActiResource()
{
    QMutexLocker lk( &m_dioMutex );
    for( std::map<quint64, TriggerOutputTask>::iterator
        it = m_triggerOutputTasks.begin();
        it != m_triggerOutputTasks.end();
         )
    {
        const quint64 taskID = it->first;
        m_triggerOutputTasks.erase( it++ );
        lk.unlock();
        TimerManager::instance()->joinAndDeleteTimer( taskID );
        lk.relock();
    }
}

void QnActiResource::setEventPort(int eventPort) {
    actiEventPort = eventPort;
}

int QnActiResource::eventPort() {
    return actiEventPort;
}

QString QnActiResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnActiResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnActiResource::createLiveDataProvider()
{
    return new QnActiStreamReader(toSharedPointer());
}

QSize QnActiResource::extractResolution(const QByteArray& resolutionStr) const
{
    QList<QByteArray> params = resolutionStr.split('x');
    if (params.size() < 2 || params[0].isEmpty() || params[1].isEmpty())
        return QSize();
    bool isDigit = params[0].at(0) >= '0' && params[0].at(0) <= '9';
    if (!isDigit)
        params[0] = params[0].mid(1);
    
    return QSize(params[0].trimmed().toInt(), params[1].trimmed().toInt());
}

QByteArray QnActiResource::unquoteStr(const QByteArray& v)
{
    QByteArray value = v.trimmed();
    int pos1 = value.startsWith('\'') ? 1 : 0;
    int pos2 = value.endsWith('\'') ? 1 : 0;
    return value.mid(pos1, value.length()-pos1-pos2);
}

QByteArray QnActiResource::makeActiRequest(const QString& group, const QString& command, CLHttpStatus& status, bool keepAllData, QString* const localAddress) const
{
    QByteArray result;

    QUrl url(getUrl());
    QAuthenticator auth = getAuth();
    CLSimpleHTTPClient client(url.host(), url.port(80), TCP_TIMEOUT, QAuthenticator());
    QString pattern(QLatin1String("cgi-bin/%1?USER=%2&PWD=%3&%4"));
    status = client.doGET(pattern.arg(group).arg(auth.user()).arg(auth.password()).arg(command));
    if (status == CL_HTTP_SUCCESS) {
        if( localAddress )
            *localAddress = client.localAddress();
        client.readAll(result);
        if (result.startsWith("ERROR: bad account")) {
            status = CL_HTTP_AUTH_REQUIRED; 
            return QByteArray();
        }
    }
    if (keepAllData)
        return result;
    else
        return unquoteStr(result.mid(result.indexOf('=')+1).trimmed());
}

static bool resolutionGreaterThan(const QSize &s1, const QSize &s2)
{
    long long res1 = s1.width() * s1.height();
    long long res2 = s2.width() * s2.height();
    return res1 > res2? true: (res1 == res2 && s1.width() > s2.width()? true: false);
}

QList<QSize> QnActiResource::parseResolutionStr(const QByteArray& resolutions)
{
    QList<QSize> result;
    QList<QSize> availResolutions;
    for(const QByteArray& r: resolutions.split(','))
        result << extractResolution(r);
    qSort(result.begin(), result.end(), resolutionGreaterThan);
    return result;
}

QMap<QByteArray, QByteArray> QnActiResource::parseSystemInfo(const QByteArray& report) const
{
    QMap<QByteArray, QByteArray> result;
    QList<QByteArray> lines = report.split('\n');
    for(const QByteArray& line: lines) {
        QList<QByteArray> tmp = line.split('=');
        result.insert(tmp[0].trimmed().toLower(), tmp.size() >= 2 ? tmp[1].trimmed() : "");
    }

    return result;
}

static const QLatin1String CAMERA_EVENT_ACTIVATED_PARAM_NAME( "activated" );
static const QLatin1String CAMERA_EVENT_DEACTIVATED_PARAM_NAME( "deactivated" );
static const QLatin1String CAMERA_INPUT_NUMBER_PARAM_NAME( "di" );

void QnActiResource::cameraMessageReceived( const QString& path, const QnRequestParamList& /*params*/ )
{
    const QStringList& pathItems = path.split(QLatin1Char('/'));
    if( pathItems.isEmpty() )
        return;

    QString inputNumber;
    bool isActivated = false;
    for( QStringList::const_iterator
        it = pathItems.begin();
        it != pathItems.end();
        ++it )
    {
        const QStringList& tokens = it->split(QLatin1Char('='));
        if( tokens.isEmpty() )
            continue;

        if( tokens[0] == CAMERA_EVENT_ACTIVATED_PARAM_NAME )
            isActivated = true;
        else if( tokens[0] == CAMERA_EVENT_DEACTIVATED_PARAM_NAME )
            isActivated = false;
        else if( tokens.size() > 1 && tokens[0] == CAMERA_INPUT_NUMBER_PARAM_NAME )
            inputNumber = tokens[1];
    }

    emit cameraInput(
        toSharedPointer(),
        inputNumber,
        isActivated,
        QDateTime::currentMSecsSinceEpoch() );
}

QList<int> QnActiResource::parseVideoBitrateCap(const QByteArray& bitrateCap) const
{
    QList<int> result;
    for(QByteArray bitrate: bitrateCap.split(','))
    {
        bitrate = bitrate.trimmed().toUpper();
        int coeff = 1;
        if (bitrate.endsWith("M"))
            coeff = 1000;
        bitrate.chop(1);
        result << bitrate.toFloat()*coeff;
    }
    return result;
}

bool QnActiResource::isRtspAudioSupported(const QByteArray& platform, const QByteArray& firmware) const
{
    QByteArray rtspAudio[][2] = 
    {
        {"T",  "4.13"}, // Platform and minimum allowed firmware version
        {"K",  "5.08"},
        {"A1", "6.03"},
        {"D", "6.03"},
        {"E", "6.03"},
        {"B", "6.03"},
        {"I", "6.03"}
    };

    QByteArray version;
    QList<QByteArray> parts = firmware.split('-');
    for (int i = 0; i < parts.size(); ++i)
    {
        if (parts[i].toLower().startsWith('v'))
        {
            version = parts[i].mid(1);
            break;
        }
    }

    for (uint i = 0; i < sizeof(rtspAudio) / sizeof(rtspAudio[0]); ++i)
    {
        if (platform == rtspAudio[i][0] && version >= rtspAudio[i][1])
            return true;
    }

    return false;
}

CameraDiagnostics::Result QnActiResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();

    CLHttpStatus status;
        
    QByteArray resolutions= makeActiRequest(QLatin1String("system"), QLatin1String("VIDEO_RESOLUTION_CAP"), status);

    if (status == CL_HTTP_AUTH_REQUIRED) 
        setStatus(Qn::Unauthorized);
    if (status != CL_HTTP_SUCCESS)
        return CameraDiagnostics::UnknownErrorResult();

    QByteArray serverReport = makeActiRequest(QLatin1String("system"), QLatin1String("SYSTEM_INFO"), status, true);
    if (status != CL_HTTP_SUCCESS)
        return CameraDiagnostics::UnknownErrorResult();
    QMap<QByteArray, QByteArray> report = parseSystemInfo(serverReport);
    setFirmware(QString::fromUtf8(report.value("firmware version")));
    setMAC(QnMacAddress(QString::fromUtf8(report.value("mac address"))));
    m_platform = report.value("platform").trimmed().toUpper();

    bool dualStreaming = report.value("channels").toInt() > 1 || !report.value("video2_resolution_cap").isEmpty();

    QList<QSize> availResolutions = parseResolutionStr(resolutions);
    if (availResolutions.isEmpty() || availResolutions[0].isEmpty())
        return CameraDiagnostics::UnknownErrorResult();

    m_resolution[0] = availResolutions.first();

    if (dualStreaming) {
        resolutions = makeActiRequest(QLatin1String("system"), QLatin1String("CHANNEL=2&VIDEO_RESOLUTION_CAP"), status);
        if (status == CL_HTTP_SUCCESS)
        {
            availResolutions = parseResolutionStr(resolutions);
            int maxSecondaryRes = SECONDARY_STREAM_MAX_RESOLUTION.width()*SECONDARY_STREAM_MAX_RESOLUTION.height();

            float currentAspect = getResolutionAspectRatio(m_resolution[0]);
            m_resolution[1] = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, currentAspect, maxSecondaryRes, availResolutions);
            if (m_resolution[1] == EMPTY_RESOLUTION_PAIR)
                m_resolution[1] = getNearestResolution(SECONDARY_STREAM_DEFAULT_RESOLUTION, 0.0, maxSecondaryRes, availResolutions); // try to get resolution ignoring aspect ration
        }
    }

    makeActiRequest(QLatin1String("system"), QLatin1String("RTP_B2=1"), status); // disable extra data aka B2 frames for RTSP (disable value:1, enable: 2)
    if (status != CL_HTTP_SUCCESS)
        return CameraDiagnostics::UnknownErrorResult();

    QByteArray fpsString = makeActiRequest(QLatin1String("system"), QLatin1String("VIDEO_FPS_CAP"), status);
    if (status != CL_HTTP_SUCCESS)
        return CameraDiagnostics::UnknownErrorResult();

    QList<QByteArray> fpsList = fpsString.split(';');
    
    for (int i = 0; i < MAX_STREAMS && i < fpsList.size(); ++i) {
        QList<QByteArray> fps = fpsList[i].split(',');
        for(const QByteArray& data: fps)
            m_availFps[i] << data.toInt();
        qSort(m_availFps[i]);
    }

    QByteArray rtspPortString = makeActiRequest(QLatin1String("system"), QLatin1String("V2_PORT_RTSP"), status);
    if (status != CL_HTTP_SUCCESS)
        return CameraDiagnostics::UnknownErrorResult();
    m_rtspPort = rtspPortString.trimmed().toInt();
    if (m_rtspPort == 0)
        m_rtspPort = DEFAULT_RTSP_PORT;

    m_hasAudio = report.value("audio").toInt() > 0 && isRtspAudioSupported(m_platform, getFirmware().toUtf8());

    QByteArray bitrateCap = report.value("video_bitrate_cap");
    if (!bitrateCap.isEmpty())
        m_availBitrate = parseVideoBitrateCap(bitrateCap);

    initializeIO( report );

    QScopedPointer<QnAbstractPtzController> ptzController(createPtzControllerInternal());
    setPtzCapabilities(ptzController->getCapabilities());

    setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, m_hasAudio ? 1 : 0);
    setProperty(Qn::MAX_FPS_PARAM_NAME, getMaxFps());
    setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, !m_resolution[1].isEmpty() ? 1 : 0);

    //detecting and saving selected resolutions
    CameraMediaStreams mediaStreams;
    mediaStreams.streams.push_back( CameraMediaStreamInfo( m_resolution[0], CODEC_ID_H264 ) );
    if( !m_resolution[1].isEmpty() )
        mediaStreams.streams.push_back( CameraMediaStreamInfo( m_resolution[1], CODEC_ID_H264 ) );
    saveResolutionList( mediaStreams );

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

bool QnActiResource::startInputPortMonitoring()
{
    if( actiEventPort == 0 )
        return false;   //no http listener is present

    //considering, that we have excusive access to the camera, so rewriting existing event setup

    //monitoring all input ports
    CLHttpStatus responseStatusCode = CL_HTTP_SUCCESS;

        //setting Digital Input Active Level
            //GET /cgi-bin/cmd/encoder?EVENT_DI1&EVENT_DI2&EVENT_RSPDO1&EVENT_RSPDO2 HTTP/1.1\r\n
            //response:
            //EVENT_DI1='0,10'\n
            //EVENT_DI2='0,0'\n
            //EVENT_RSPDO1='1,0'\n
            //ERROR: EVENT_RSPDO2 not found\n

    QString eventStr;
    for( int i = 0; i < m_inputCount; ++i )
    {
        if( !eventStr.isEmpty() )
            eventStr += lit("&");
        eventStr += lit("EVENT_DI%1='0,0'").arg(i+1);
    }
    QString localInterfaceAddress;  //determining address of local interface, used to connect to camera
    QByteArray responseMsgBody = makeActiRequest(QLatin1String("encoder"), eventStr, responseStatusCode, false, &localInterfaceAddress);
    if( responseStatusCode != CL_HTTP_SUCCESS )
        return false;

    static const int EVENT_HTTP_SERVER_NUMBER = 1;
    static const int MAX_CONNECTION_TIME_SEC = 7;

        //registering itself as event server
            //GET /cgi-bin/cmd/encoder?FTP_SERVER&HTTP_SERVER&SMTP_SEC&SMTP_PRI HTTP/1.1\r\n
            //response:
            //FTP_SERVER=',,,21,0,10000'\n
            //HTTP_SERVER='1,1,192.168.0.101,3451,hz,hz,10'\n
            //HTTP_SERVER='2,0,,80,,,10'\n
            //SMTP_SEC='0,2,,,,,25,110,10'\n

            //GET /cgi-bin/cmd/encoder?HTTP_SERVER=1,1,192.168.0.101,3451,hz,hzhz,10 HTTP/1.1\r\n
            //response:
            //OK: HTTP_SERVER='1,1,192.168.0.101,3451,hz,hzhz,10'\n
    responseMsgBody = makeActiRequest(
        QLatin1String("encoder"),
        lit("HTTP_SERVER=%1,1,%2,%3,guest,guest,%4").arg(EVENT_HTTP_SERVER_NUMBER).arg(localInterfaceAddress).arg(actiEventPort).arg(MAX_CONNECTION_TIME_SEC),
        responseStatusCode );
    if( responseStatusCode != CL_HTTP_SUCCESS )
        return false;

    //registering URL commands (one command per input port)
        //GET /cgi-bin/cmd/encoder?EVENT_RSPCMD1=1,[api/camera_event/98/di/activated],[api/camera_event/98/di/deactivated]&EVENT_RSPCMD2=1,[],[]&EVENT_RSPCMD3=1,[],[]

    const QString cgiPath = lit("api/camera_event/%1/di").arg(this->getId().toString());
    QString setupURLCommandRequestStr;
    for( int i = 1; i <= m_inputCount; ++i )
    {
        if( !setupURLCommandRequestStr.isEmpty() )
            setupURLCommandRequestStr += QLatin1String("&");
        setupURLCommandRequestStr += lit("EVENT_RSPCMD%1=%2,[%3/%4/di=%1],[%3/%5/di=%1]").arg(i).arg(EVENT_HTTP_SERVER_NUMBER).arg(cgiPath).arg(CAMERA_EVENT_ACTIVATED_PARAM_NAME).arg(CAMERA_EVENT_DEACTIVATED_PARAM_NAME);
    }
    responseMsgBody = makeActiRequest(
        QLatin1String("encoder"),
        setupURLCommandRequestStr,
        responseStatusCode );
    if( responseStatusCode != CL_HTTP_SUCCESS )
        return false;


        //registering events (one event per input port)
            //GET /cgi-bin/cmd/encoder?EVENT_CONFIG HTTP/1.1\r\n
            //response:
            //EVENT_CONFIG='1,1,1234567,00:00,24:00,DI1,MSG1'\n
            //EVENT_CONFIG='2,0,1234567,00:00,24:00,NONE,NONE'\n
            //EVENT_CONFIG='3,0,1234567,00:00,24:00,NONE,NONE'\n

            //GET /cgi-bin/cmd/encoder?EVENT_CONFIG=1,1,1234567,00:00,24:00,DI1,MSG1 HTTP/1.1\r\n
            //response:
            //OK: EVENT_CONFIG='1,1,1234567,00:00,24:00,DI1,MSG1'\n
    QString registerEventRequestStr;
    for( int i = 1; i <= m_inputCount; ++i )
    {
        if( !registerEventRequestStr.isEmpty() )
            registerEventRequestStr += QLatin1String("&");
        registerEventRequestStr += lit("EVENT_CONFIG=%1,1,1234567,00:00,24:00,DI%1,CMD%1").arg(i);
    }
    responseMsgBody = makeActiRequest(
        QLatin1String("encoder"),
        registerEventRequestStr,
        responseStatusCode );
    if( responseStatusCode != CL_HTTP_SUCCESS )
        return false;

    m_inputMonitored = true;

    return true;
}

void QnActiResource::stopInputPortMonitoring()
{
    if( actiEventPort == 0 )
        return;   //no http listener is present

        //unregistering events
    QString registerEventRequestStr;
    for( int i = 1; i <= m_inputCount; ++i )
    {
        if( !registerEventRequestStr.isEmpty() )
            registerEventRequestStr += QLatin1String("&");
        registerEventRequestStr += lit("EVENT_CONFIG=%1,0,1234567,00:00,24:00,DI%1,CMD%1").arg(i);
    }
    CLHttpStatus responseStatusCode = CL_HTTP_SUCCESS;
    makeActiRequest(
        QLatin1String("encoder"),
        registerEventRequestStr,
        responseStatusCode );

    m_inputMonitored = false;
}

bool QnActiResource::isInputPortMonitored() const
{
    return m_inputMonitored;
}

QnConstResourceAudioLayoutPtr QnActiResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    if (isAudioEnabled()) {
        const QnActiStreamReader* actiReader = dynamic_cast<const QnActiStreamReader*>(dataProvider);
        if (actiReader && actiReader->getDPAudioLayout())
            return actiReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

QString QnActiResource::getRtspUrl(int actiChannelNum) const
{
    QUrl url(getUrl());
    url.setScheme(QLatin1String("rtsp"));
    url.setPort(m_rtspPort);
    if (isAudioSupported())
        url.setPath(QString(QLatin1String("/stream%1/")).arg(actiChannelNum));
    else
        url.setPath(QString(QLatin1String("/track%1/")).arg(actiChannelNum));
    return url.toString();
}

int QnActiResource::getMaxFps() const
{
    return m_availFps[0].last();
}

QSize QnActiResource::getResolution(Qn::ConnectionRole role) const
{
    return (role == Qn::CR_LiveVideo ? m_resolution[0] : m_resolution[1]);
}

int QnActiResource::roundFps(int srcFps, Qn::ConnectionRole role) const
{
    QList<int> availFps = (role == Qn::CR_LiveVideo ? m_availFps[0] : m_availFps[1]);
    int minDistance = INT_MAX;
    int result = srcFps;
    for (int i = 0; i < availFps.size(); ++i)
    {
        int distance = qAbs(availFps[i] - srcFps);
        if (distance <= minDistance) { // preffer higher fps if same distance
            minDistance = distance;
            result = availFps[i];
        }
    }

    return result;
}

int QnActiResource::roundBitrate(int srcBitrateKbps) const
{
    int minDistance = INT_MAX;
    int result = srcBitrateKbps;
    for (int i = 0; i < m_availBitrate.size(); ++i)
    {
        int distance = qAbs(m_availBitrate[i] - srcBitrateKbps);
        if (distance <= minDistance) { // preffer higher bitrate if same distance
            minDistance = distance;
            result = m_availBitrate[i];
        }
    }

    return result;
}

bool QnActiResource::isAudioSupported() const
{
    return m_hasAudio;
}

bool QnActiResource::hasDualStreaming() const
{
    return getProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME).toInt() > 0;
}

QnAbstractPtzController *QnActiResource::createPtzControllerInternal()
{
    return new QnActiPtzController(toSharedPointer(this));
}

QStringList QnActiResource::getRelayOutputList() const
{
    QStringList outputIDStrList;
    for( int i = 1; i <= m_outputCount; ++i )
        outputIDStrList << QString::number(i);
    return outputIDStrList;
}

QStringList QnActiResource::getInputPortList() const
{
    QStringList inputIDStrList;
    for( int i = 1; i <= m_inputCount; ++i )
        inputIDStrList << QString::number(i);
    return inputIDStrList;
}

static const int MIN_DIO_PORT_NUMBER = 1;
static const int MAX_DIO_PORT_NUMBER = 2;

bool QnActiResource::setRelayOutputState(
    const QString& ouputID,
    bool activate,
    unsigned int autoResetTimeoutMS )
{
    QMutexLocker lk( &m_dioMutex );

    bool outputNumberOK = true;
    const int outputNumber = !ouputID.isEmpty() ? ouputID.toInt(&outputNumberOK) : 1;
    if( !outputNumberOK )
        return false;
    if( outputNumber < MIN_DIO_PORT_NUMBER || outputNumber > m_outputCount )
        return false;

    const quint64 timerID = TimerManager::instance()->addTimer( this, 0 );
    m_triggerOutputTasks.insert( std::make_pair( timerID, TriggerOutputTask( outputNumber, activate, autoResetTimeoutMS ) ) );
    return true;
}

void QnActiResource::onTimer( const quint64& timerID )
{
    TriggerOutputTask triggerOutputTask;
    {
        QMutexLocker lk( &m_dioMutex );
        std::map<quint64, TriggerOutputTask>::iterator it = m_triggerOutputTasks.find( timerID );
        if( it == m_triggerOutputTasks.end() )
            return;

        triggerOutputTask = it->second;
        m_triggerOutputTasks.erase( it );
    }

    const int dioOutputMask =
        1 << (4 + triggerOutputTask.outputID - 1) | //signalling output port id we refer to
        (triggerOutputTask.active ? (1 << (triggerOutputTask.outputID-1)) : 0);        //signalling new port state

    CLHttpStatus status = CL_HTTP_SUCCESS;
    QByteArray dioResponse = makeActiRequest(
        QLatin1String("encoder"),
        lit("DIO_OUTPUT=0x%1").arg(dioOutputMask, 2, 16, QLatin1Char('0')),
        status );
    if( status != CL_HTTP_SUCCESS )
        return;

    if( triggerOutputTask.autoResetTimeoutMS > 0 )
        m_triggerOutputTasks.insert( std::make_pair(
            TimerManager::instance()->addTimer( this, triggerOutputTask.autoResetTimeoutMS ),
            TriggerOutputTask( triggerOutputTask.outputID, !triggerOutputTask.active, 0 ) ) );
}

void QnActiResource::initializeIO( const QMap<QByteArray, QByteArray>& systemInfo )
{
    QMap<QByteArray, QByteArray>::const_iterator it = systemInfo.find( "di" );
    if( it != systemInfo.end() )
        m_inputCount = it.value().toInt();
    if( m_inputCount > 0 )
        setCameraCapability(Qn::RelayInputCapability, true);

    it = systemInfo.find( "do" );
    if( it != systemInfo.end() )
        m_outputCount = it.value().toInt();
    if( m_outputCount > 0 )
        setCameraCapability(Qn::RelayOutputCapability, true);
}

#endif // #ifdef ENABLE_ACTI
