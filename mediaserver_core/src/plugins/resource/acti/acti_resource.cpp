#ifdef ENABLE_ACTI
#include "acti_resource.h"

#include <functional>
#include <memory>

#include <business/business_event_rule.h>
#include <nx_ec/dummy_handler.h>

#include "api/app_server_connection.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "acti_stream_reader.h"
#include "utils/common/synctime.h"
#include "acti_ptz_controller.h"
#include "rest/server/rest_connection_processor.h"
#include "common/common_module.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"


const QString QnActiResource::MANUFACTURE(lit("ACTI"));
const QString QnActiResource::CAMERA_PARAMETER_GROUP_ENCODER(lit("encoder"));
const QString QnActiResource::CAMERA_PARAMETER_GROUP_SYSTEM(lit("system"));
const QString QnActiResource::CAMERA_PARAMETER_GROUP_DEFAULT(lit("encoder"));
const QString QnActiResource::DEFAULT_ADVANCED_PARAMETERS_TEMPLATE(lit("nx-cube.xml"));
const QString QnActiResource::ADVANCED_PARAMETERS_TEMPLATE_PARAMETER_NAME(lit("advancedParametersTemplate"));
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

    setDefaultAuth(QLatin1String("admin"), QLatin1String("123456"));
    for (uint i = 0; i < sizeof(DEFAULT_AVAIL_BITRATE_KBPS)/sizeof(int); ++i)
        m_availBitrate << DEFAULT_AVAIL_BITRATE_KBPS[i];
}

QnActiResource::~QnActiResource()
{
    QnMutexLocker lk( &m_dioMutex );
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

void QnActiResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    QUrl apiUrl;
    apiUrl.setScheme( lit("http") );
    apiUrl.setHost( getHostAddress() );
    apiUrl.setPort( QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT) );
    apiUrl.setUserName( getAuth().user() );
    apiUrl.setPassword( getAuth().password() );
    apiUrl.setPath( lit("/cgi-bin/system") );
    apiUrl.setQuery( lit("USER=%1&PWD=%2&SYSTEM_INFO").arg(getAuth().user()).arg(getAuth().password()) );

    QString resourceMac = getMAC().toString();
    auto requestCompletionFunc = [resourceMac, completionHandler]
        ( SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody ) mutable
    {
        if( osErrorCode != SystemError::noError ||
            statusCode != nx_http::StatusCode::ok )
        {
            return completionHandler( false );
        }

        if( msgBody.startsWith("ERROR: bad account") )
            return completionHandler( false );

        QMap<QByteArray, QByteArray> report = QnActiResource::parseSystemInfo( msgBody );
        QByteArray mac = report.value("mac address");
        mac.replace( ':', '-' );
        completionHandler( mac == resourceMac.toLatin1() );
    };

    nx_http::downloadFileAsync(
        apiUrl,
        requestCompletionFunc );
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
    status = makeActiRequest( getUrl(), getAuth(), group, command, keepAllData, &result, localAddress );
    return result;
}

CLHttpStatus QnActiResource::makeActiRequest(
    const QUrl& url,
    const QAuthenticator& auth,
    const QString& group,
    const QString& command,
    bool keepAllData,
    QByteArray* const msgBody,
    QString* const localAddress )
{
    CLSimpleHTTPClient client(url.host(), url.port(nx_http::DEFAULT_HTTP_PORT), TCP_TIMEOUT, QAuthenticator());
    QString pattern(QLatin1String("cgi-bin/%1?USER=%2&PWD=%3&%4"));
    CLHttpStatus status = client.doGET(pattern.arg(group).arg(auth.user()).arg(auth.password()).arg(command));
    if (status == CL_HTTP_SUCCESS) {
        if( localAddress )
            *localAddress = client.localAddress();
        msgBody->clear();
        client.readAll(*msgBody);
        if (msgBody->startsWith("ERROR: bad account"))
            return CL_HTTP_AUTH_REQUIRED; 
    }

    if (!keepAllData)
        *msgBody = unquoteStr(msgBody->mid(msgBody->indexOf('=')+1).trimmed());
    return status;
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

QMap<QByteArray, QByteArray> QnActiResource::parseSystemInfo(const QByteArray& report)
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
        qnSyncTime->currentUSecsSinceEpoch() );
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

    bool dualStreaming = report.value("channels").toInt() > 1 ||
            !report.value("video2_resolution_cap").isEmpty() ||
            report.value("streaming_mode_cap").toLower() == "dual";

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

    fetchAndSetAdvancedParameters();

    setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, m_hasAudio ? 1 : 0);
    setProperty(Qn::MAX_FPS_PARAM_NAME, getMaxFps());
    setProperty(Qn::HAS_DUAL_STREAMING_PARAM_NAME, !m_resolution[1].isEmpty() ? 1 : 0);
    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

bool QnActiResource::SetupAudioInput()
{
    if (!isAudioSupported())
        return true;
    bool value = isAudioEnabled();
    QnMutexLocker lock(&m_audioCfgMutex);
    if (value == m_audioInputOn)
        return true;
    CLHttpStatus status;
    makeActiRequest(QLatin1String("system"), lit("V2_AUDIO_ENABLED=%1").arg(value ? lit("1") : lit("0")), status);
    if (status == CL_HTTP_SUCCESS) {
        m_audioInputOn = value;
        return true;
    }
    else {
        return false;
    }
}


bool QnActiResource::startInputPortMonitoringAsync( std::function<void(bool)>&& /*completionHandler*/ )
{
    if( hasFlags(Qn::foreigner) ||      //we do not own camera
        !hasCameraCapabilities(Qn::RelayInputCapability) )
    {
        return false;
    }

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

    const QString cgiPath = lit("/api/camera_event/%1/di").arg(this->getId().toString());
    QString setupURLCommandRequestStr;
    for( int i = 1; i <= m_inputCount; ++i )
    {
        if( !setupURLCommandRequestStr.isEmpty() )
            setupURLCommandRequestStr += QLatin1String("&");
        setupURLCommandRequestStr += lit("EVENT_RSPCMD%1=%2,[%3/%4/di=%1],[%3/%5/di=%1]")
                .arg(i)
                .arg(EVENT_HTTP_SERVER_NUMBER)
                .arg(cgiPath)
                .arg(CAMERA_EVENT_ACTIVATED_PARAM_NAME)
                .arg(CAMERA_EVENT_DEACTIVATED_PARAM_NAME);
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

void QnActiResource::stopInputPortMonitoringAsync()
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
    m_inputMonitored = false;

    const QAuthenticator auth = getAuth();
    QUrl url = getUrl();
    url.setPath( lit("/cgi-bin/%1?USER=%2&PWD=%3&%4").arg(lit("encoder")).arg(auth.user()).arg(auth.password()).arg(registerEventRequestStr) );
    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    //TODO #ak do not use DummyHandler here. httpClient->doGet should accept functor
    connect( httpClient.get(), &nx_http::AsyncHttpClient::done,
        ec2::DummyHandler::instance(), [httpClient](nx_http::AsyncHttpClientPtr) mutable {
            httpClient->disconnect( nullptr, (const char*)nullptr );
            httpClient.reset();
        },
        Qt::DirectConnection );
    httpClient->doGet( url );
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

QnIOPortDataList QnActiResource::getRelayOutputList() const
{
    QnIOPortDataList outputIDStrList;
    for( int i = 1; i <= m_outputCount; ++i ) {
        QnIOPortData value;
        value.portType = Qn::PT_Output;
        value.id = QString::number(i);
        value.outputName = tr("Output %1").arg(i);
        outputIDStrList.push_back(value);
    }
    return outputIDStrList;
}

QnIOPortDataList QnActiResource::getInputPortList() const
{
    QnIOPortDataList inputIDStrList;
    for( int i = 1; i <= m_inputCount; ++i ) {
        QnIOPortData value;
        value.portType = Qn::PT_Input;
        value.id = QString::number(i);
        value.inputName = tr("Input %1").arg(i);
        inputIDStrList.push_back(value);
    }
    return inputIDStrList;
}

static const int MIN_DIO_PORT_NUMBER = 1;
static const int MAX_DIO_PORT_NUMBER = 2;

bool QnActiResource::setRelayOutputState(
    const QString& ouputID,
    bool activate,
    unsigned int autoResetTimeoutMS )
{
    QnMutexLocker lk( &m_dioMutex );

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
        QnMutexLocker lk( &m_dioMutex );
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

bool QnActiResource::getParamPhysical(const QString& id, QString& value)
{
    QSet<QString> idList = {id};
    QnCameraAdvancedParamValueList result;
    if(!getParamsPhysical(idList, result))
        return false;

    auto it = std::find_if(result.cbegin(), result.cend(), [&id](const QnCameraAdvancedParamValue& paramValue) {
        return paramValue.id == id;
    });

    if(it == result.cend())
        return false;

    value = it->value;
    return true;
}

bool QnActiResource::setParamPhysical(const QString& id, const QString& value)
{
    QnCameraAdvancedParamValueList values = {QnCameraAdvancedParamValue(id, value)};
    QnCameraAdvancedParamValueList result;

    return setParamsPhysical(values, result);
}

bool QnActiResource::getParamsPhysical(const QSet<QString> &idList, QnCameraAdvancedParamValueList &result)
{
    bool success = true;
    const auto params = getParamsByIds(idList);
    const auto queries = buildGetParamsQueries(params);
    const auto queriesResults = executeParamsQueries(queries, success);
    parseParamsQueriesResult(queriesResults, params, result);

    return result.size() == idList.size();
}

bool QnActiResource::setParamsPhysical(const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result)
{
    bool success;
    QSet<QString> idList;

    for(const auto& value: values)
        idList.insert(value.id);

    const auto params = getParamsByIds(idList);
    const auto queries = buildSetParamsQueries(values);
    const auto queriesResults = executeParamsQueries(queries, success);
    parseParamsQueriesResult(queriesResults, params, result);

    const auto maintenanceQueries = buildMaintenanceQueries(values);
    if(!maintenanceQueries.empty())
    {
        executeParamsQueries(maintenanceQueries, success);
        return success;
    }

    return result.size() == values.size();
}

/*
 * Operations with maintenance params should be performed when every other param is already changed.
 * Also maintenance params must not get into queries that obtain param values.
 */
bool QnActiResource::isMaintenanceParam(const QnCameraAdvancedParameter &param) const
{
    return param.dataType == QnCameraAdvancedParameter::DataType::Button;
}

QList<QnCameraAdvancedParameter> QnActiResource::getParamsByIds(const QSet<QString>& idList) const
{
    QList<QnCameraAdvancedParameter> params;
    for(const auto& id: idList)
    {
        auto param = m_advancedParameters.getParameterById(id);
        params.append(param);
    }
    return  params;
}

QMap<QString, QnCameraAdvancedParameter> QnActiResource::getParamsMap(const QSet<QString> &idList) const
{
    QMap<QString, QnCameraAdvancedParameter> params;
    for(const auto& id: idList)
        params[id] = m_advancedParameters.getParameterById(id);

    return  params;
}

/*
 * Replaces placeholders in param query with actual values retrieved from camera.
 * Needed when user changes not all params in agregate parameter.
 * Example: WB_GAIN=127,%WB_R_GAIN becomes WB_GAIN=127,245
 */
QMap<QString, QString> QnActiResource::resolveQueries(QMap<QString, QnCameraAdvancedParamQueryInfo> &queries) const
{
    CLHttpStatus status;
    QMap<QString, QString> setQueries;
    QMap<QString, QString> queriesToResolve;

    for(const auto& agregate: queries.keys())
        if(queries[agregate].cmd.indexOf('%') != -1)
            queriesToResolve[queries[agregate].group] += agregate + lit("&");

    QMap<QString, QString> respParams;
    for(const auto& group: queriesToResolve.keys())
    {
        auto response = makeActiRequest(group, queriesToResolve[group], status, true);
        parseCameraParametersResponse(response, respParams);
    }

    for(const auto& agregateName : respParams.keys())
        queries[agregateName].cmd = fillMissingParams(
            queries[agregateName].cmd,
            respParams[agregateName]);

    for(const auto& agregate: queries.keys())
        setQueries[queries[agregate].group] += agregate + lit("=") + queries[agregate].cmd + lit("&");

    return setQueries;
}

/*
 * Used by resolveQueries function. Performs opertaions with parameter strings
 */
QString QnActiResource::fillMissingParams(const QString &unresolvedTemplate, const QString &valueFromCamera) const
{
    auto templateValues = unresolvedTemplate.split(",");
    auto cameraValues = valueFromCamera.split(",");

    for(size_t i = 0; i < templateValues.size(); i++)
    {
        if(i >= cameraValues.size())
            break;
        else if(templateValues.at(i).startsWith("%"))
            templateValues[i] = cameraValues.at(i);
    }

    return templateValues.join(',');
}

QMap<QString, QString> QnActiResource::buildGetParamsQueries(const QList<QnCameraAdvancedParameter> &params) const
{
    QSet<QString> agregates;
    QMap<QString, QString> queries;

    for(const auto& param: params)
    {
        if(param.dataType == QnCameraAdvancedParameter::DataType::Button)
            continue;

        auto group = getParamGroup(param);
        auto cmd = getParamCmd(param);

        auto split = cmd.split('=');
        if(split.size() > 1 && !agregates.contains(split[0]))
        {
            queries[group] += split[0] + lit("&");
            agregates.insert(split[0]);
        }
    }

    return queries;
}

QMap<QString, QString> QnActiResource::buildSetParamsQueries(const QnCameraAdvancedParamValueList &values) const
{
    QMap<QString, QString> paramToAgregate;
    QMap<QString, QString> paramToValue;
    QMap<QString, QnCameraAdvancedParamQueryInfo> agregateToCmd;

    for(const auto& val: values)
        paramToValue[val.id] = val.value;

    const auto paramsMap = getParamsMap(paramToValue.keys().toSet());

    for(const auto& id: paramsMap.keys())
    {
        if(isMaintenanceParam(paramsMap[id]))
        {
            paramToValue.remove(id);
            continue;
        }

        auto split = getParamCmd(paramsMap[id]).split('=');

        if(split.size() < 2)
            continue;

        auto agregate = split[0].trimmed();
        auto args = split[1].trimmed();

        paramToAgregate[id] = agregate;

        QnCameraAdvancedParamQueryInfo info;
        info.group = getParamGroup(paramsMap[id]);
        info.cmd = args;
        agregateToCmd[agregate] = info;
    }

    for(const auto& paramId: paramToValue.keys())
    {
        auto agregate = paramToAgregate[paramId];
        paramToValue[paramId] = convertParamValueToDeviceFormat( paramToValue[paramId], paramsMap[paramId]);
        agregateToCmd[agregate].cmd = agregateToCmd[agregate].cmd
            .replace(lit("%") + paramId, paramToValue[paramId]);
    }
    return resolveQueries(agregateToCmd);
}

QMap<QString, QString> QnActiResource::buildMaintenanceQueries(const QnCameraAdvancedParamValueList &values) const
{
    QSet<QString> paramsIds;
    QMap<QString, QString> queries;
    for(const auto& val: values)
        paramsIds.insert(val.id);

    auto params = getParamsByIds(paramsIds);

    for(const auto& param: params)
        if(isMaintenanceParam(param))
            queries[getParamGroup(param)] += param.id + lit("&");

    return queries;
}

/*
 * Executes the queries, parses the response and returns map (paramName => paramValue).
 * paramName and paramValue are raw strings that we got from camera (no parsing of agregate params).
 */
QMap<QString, QString> QnActiResource::executeParamsQueries(const QMap<QString, QString>& queries, bool& isSuccessful) const
{
    CLHttpStatus status;
    QMap<QString, QString> result;
    isSuccessful = true;
    for(const auto& q: queries.keys())
    {
        auto response = makeActiRequest(q, queries[q], status, true);
        if(status != CL_HTTP_SUCCESS)
            isSuccessful = false;
        parseCameraParametersResponse(response, result);
    }

    return result;
}

/*
 * Retrieves needed params from queries result and put them  to the list.
 * This function performs parsing of agregate params.
 */
void QnActiResource::parseParamsQueriesResult(
    const QMap<QString, QString> &queriesResult,
    const QList<QnCameraAdvancedParameter> &params,
    QnCameraAdvancedParamValueList &result) const
{

    QMap<QString, QString> parsed;
    for(const auto& param: params)
    {
        auto cmd = getParamCmd(param);
        if(!parsed.contains(param.id))
        {
            auto split = cmd.split("=");

            if(split.size() != 2)
                continue;

            auto mainParam = split[0].trimmed();
            auto mask = split[1].trimmed();
            if(queriesResult.contains(mainParam))
                extractParamValues(queriesResult[mainParam], mask, parsed);
        }

        if(parsed.contains(param.id))
        {
            parsed[param.id] = convertParamValueFromDeviceFormat(parsed[param.id], param);
            result.append(QnCameraAdvancedParamValue(param.id, parsed[param.id]));
        }
    }
}


QString QnActiResource::getParamCmd(const QnCameraAdvancedParameter &param) const
{
    return param.writeCmd.isEmpty() ?
        param.id + lit("=%") + param.id :
        param.writeCmd;

}

/*
 * Extracts params from real string from camera with given mask.
 * Example: paramValue = "1,2,3", mask="%param1,%param2".
 * Two items will be added to map: param1 => "1" , param2 => "2,3"
 */
void QnActiResource::extractParamValues(const QString &paramValue, const QString &mask, QMap<QString, QString>& result) const
{

    const auto paramNames = mask.split(',');
    const auto paramValues = paramValue.split(',');
    const auto paramCount = paramNames.size();

    for(size_t i = 0; i < paramCount; i++)
    {
        auto name = paramNames.at(i).mid(1);
        if(i < paramCount - 1)
            result[name] = paramValues.at(i);
        else
            result[name] = paramValues.mid(i).join(',');
    }
}

QString QnActiResource::getParamGroup(const QnCameraAdvancedParameter &param) const
{
    return param.readCmd.isEmpty() ? CAMERA_PARAMETER_GROUP_DEFAULT : param.readCmd;
}

bool QnActiResource::parseParameter(const QString& paramStr, QnCameraAdvancedParamValue& value) const
{
    bool success = false;
    auto split = paramStr.trimmed().split('=');

    if(split.size() == 2)
    {
        success = true;
        if(split[0].startsWith("OK:"))
            value.id = split[0].split(' ')[1].trimmed();
        else
            value.id = split[0];

        value.value = split[1].mid(1, split[1].size()-2);
    }

    return success;
}

void QnActiResource::parseCameraParametersResponse(const QByteArray& response, QnCameraAdvancedParamValueList& result) const
{
    auto lines = response.split('\n');
    for(const auto& line: lines)
    {
        if(line.startsWith("ERROR:"))
            continue;
        QnCameraAdvancedParamValue param;
        if(parseParameter(line, param))
            result.append(param);
    }
}

void QnActiResource::parseCameraParametersResponse(const QByteArray& response, QMap<QString, QString>& result) const
{
    auto lines = response.split('\n');
    for(const auto& line: lines)
    {
        if(line.startsWith("ERROR:"))
            continue;
        QnCameraAdvancedParamValue param;
        if(parseParameter(line, param))
            result[param.id] = param.value;
    }
}

QString QnActiResource::convertParamValueToDeviceFormat(const QString &paramValue, const QnCameraAdvancedParameter &param) const
{
    auto tmp = paramValue;

    if(param.dataType == QnCameraAdvancedParameter::DataType::Enumeration)
        tmp = param.toInternalRange(paramValue).replace('|', ',');
    else if(param.dataType == QnCameraAdvancedParameter::DataType::Bool)
        tmp = (paramValue == lit("true") ?  lit("1") : lit("0"));

    return tmp;
}

QString QnActiResource::convertParamValueFromDeviceFormat(const QString &paramValue, const QnCameraAdvancedParameter &param) const
{
    QString tmp = paramValue;
    if(param.dataType == QnCameraAdvancedParameter::DataType::Enumeration )
    {
        tmp = tmp.replace(',', '|');
        tmp = param.fromInternalRange(
            param.fromInternalRange(tmp).isEmpty() ? paramValue : tmp);
    }
    else if(param.dataType == QnCameraAdvancedParameter::DataType::Bool)
    {
        tmp = (paramValue == lit("1") ?  lit("true") : lit("false"));
    }

    return tmp;
}

bool QnActiResource::loadAdvancedParametersTemplateFromFile(QnCameraAdvancedParams& params, const QString& templateFilename)
{
    QFile paramsTemplateFile(templateFilename);
#ifdef _DEBUG
    QnCameraAdvacedParamsXmlParser::validateXml(&paramsTemplateFile);
#endif
    bool result = QnCameraAdvacedParamsXmlParser::readXml(&paramsTemplateFile, params);
#ifdef _DEBUG
    if (!result)
        qWarning() << "Error while parsing xml" << templateFilename;
#endif
    return result;
}

void QnActiResource::fetchAndSetAdvancedParameters()
{
    QnMutexLocker lock( &m_physicalParamsMutex );
    m_advancedParameters.clear();

    auto templateFile = getAdvancedParametersTemplate();
    QnCameraAdvancedParams params;
    if (!loadAdvancedParametersTemplateFromFile(
            params,
            lit(":/camera_advanced_params/") + templateFile))
        return;

    QSet<QString> supportedParams = calculateSupportedAdvancedParameters(params);
    m_advancedParameters = params.filtered(supportedParams);
    QnCameraAdvancedParamsReader::setParamsToResource(this->toSharedPointer(), m_advancedParameters);
}

QString QnActiResource::getAdvancedParametersTemplate() const
{
    QnResourceData resourceData = qnCommon->dataPool()->data(getVendor(), getModel());
    auto advancedParametersTemplate = resourceData.value<QString>(ADVANCED_PARAMETERS_TEMPLATE_PARAMETER_NAME);
    return advancedParametersTemplate.isEmpty() ?
        DEFAULT_ADVANCED_PARAMETERS_TEMPLATE : advancedParametersTemplate;
}

QSet<QString> QnActiResource::calculateSupportedAdvancedParameters(const QnCameraAdvancedParams &allParams) const
{
    QList<QnCameraAdvancedParameter> paramsList;
    QSet<QString> result;
    auto paramIds = allParams.allParameterIds();
    bool success = true;

    for(const auto& id: paramIds)
    {
        auto param = allParams.getParameterById(id);
        paramsList.append(param);
        if(param.dataType == QnCameraAdvancedParameter::DataType::Button)
            result.insert(param.id);
    }

    auto queries = buildGetParamsQueries(paramsList);
    auto queriesResult = executeParamsQueries(queries, success);

    for(const auto& param: paramsList)
    {
        if(queriesResult.contains(param.id))
            result.insert(param.id);
        else
        {
            auto cmd = getParamCmd(param);
            auto agregate = cmd.split('=')[0];
            if(queriesResult.contains(agregate))
                result.insert(param.id);
        }
    }

    return result;
}

#endif // #ifdef ENABLE_ACTI
