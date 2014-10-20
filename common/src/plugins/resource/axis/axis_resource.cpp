#ifdef ENABLE_AXIS

#include "axis_resource.h"

#include <algorithm>
#include <functional>
#include <memory>

#include <api/app_server_connection.h>

#include <utils/common/synctime.h>
#include <utils/common/log.h>

#include "../onvif/dataprovider/onvif_mjpeg.h"

#include "axis_stream_reader.h"
#include "axis_ptz_controller.h"

using namespace std;

const QString QnPlAxisResource::MANUFACTURE(lit("Axis"));
static const float MAX_AR_EPS = 0.04f;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
static const quint16 DEFAULT_AXIS_API_PORT = 80;

QnPlAxisResource::QnPlAxisResource()
{
    setVendor(lit("Axis"));
    setAuth(QLatin1String("root"), QLatin1String("root"));
    m_lastMotionReadTime = 0;
}

QnPlAxisResource::~QnPlAxisResource()
{
    stopInputPortMonitoring();
}

QString QnPlAxisResource::getDriverName() const
{
    return MANUFACTURE;
}

void QnPlAxisResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlAxisResource::createLiveDataProvider()
{
    return new QnAxisStreamReader(toSharedPointer());
}

void QnPlAxisResource::setCroppingPhysical(QRect /*cropping*/)
{

}

bool QnPlAxisResource::startInputPortMonitoring()
{
    if( hasFlags(Qn::foreigner)      //we do not own camera
        || m_inputPortNameToIndex.empty() )
    {
        return false;
    }

    //based on VAPIX Version 3 I/O Port API

    const QAuthenticator& auth = getAuth();
    QUrl requestUrl;
    requestUrl.setHost( getHostAddress() );
    requestUrl.setPort( QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT) );
    //preparing request
    QString requestPath = lit("/axis-cgi/io/port.cgi?monitor=");
    for( std::map<QString, unsigned int>::const_iterator
        it = m_inputPortNameToIndex.cbegin();
        it != m_inputPortNameToIndex.cend();
        ++it )
    {
        if( it != m_inputPortNameToIndex.cbegin() )
            requestPath += lit(",");
        requestPath += QString::number(it->second);
    }
    requestUrl.setPath( requestPath );

    QMutexLocker lk( &m_inputPortMutex );
    nx_http::AsyncHttpClientPtr httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    connect( httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnPlAxisResource::onMonitorResponseReceived, Qt::DirectConnection );
    connect( httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable, this, &QnPlAxisResource::onMonitorMessageBodyAvailable, Qt::DirectConnection );
    connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnPlAxisResource::onMonitorConnectionClosed, Qt::DirectConnection );
    httpClient->setTotalReconnectTries( nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
    httpClient->setUserName( auth.user() );
    httpClient->setUserPassword( auth.password() );
        
    if( !httpClient->doGet( requestUrl ) )
        return false;
    
    m_inputPortHttpMonitor = std::move( httpClient );
    return true;
}

void QnPlAxisResource::stopInputPortMonitoring()
{
    QMutexLocker lk( &m_inputPortMutex );

    if( !m_inputPortHttpMonitor )
        return;

    std::shared_ptr<nx_http::AsyncHttpClient> httpClient = std::move(m_inputPortHttpMonitor);
    lk.unlock();
    httpClient->terminate();
    httpClient.reset();
    lk.relock();
}

bool QnPlAxisResource::isInputPortMonitored() const
{
    QMutexLocker lk( &m_inputPortMutex );
    return m_inputPortHttpMonitor.get() != nullptr;
}

bool QnPlAxisResource::isInitialized() const
{
    QMutexLocker lock(&m_mutex);
    return !m_resolutionList.isEmpty();
}

void QnPlAxisResource::clear()
{
    m_resolutionList.clear();
}

struct WindowInfo
{
    enum RectType {Unknown, Motion, MotionMask};
    WindowInfo(): left(-1), right(-1), top(-1), bottom(-1), rectType(Unknown) {}
    bool isValid() const 
    { 
        return left >= 0 && right >= 0 && top >= 0 && bottom >= 0 && rectType != Unknown;
    }
    QRect toRect() const
    {
        return QRect(QPoint(left, top), QPoint(right, bottom));
    }

    int left;
    int right;
    int top;
    int bottom;
    RectType rectType;
};

QRect QnPlAxisResource::axisRectToGridRect(const QRect& axisRect)
{
    qreal xCoeff = 9999.0 / (MD_WIDTH-1);
    qreal yCoeff = 9999.0 / (MD_HEIGHT-1);
    QPoint topLeft(MD_WIDTH - (axisRect.left()/xCoeff + 0.5), MD_HEIGHT - (axisRect.top()/yCoeff + 0.5));
    QPoint bottomRight(MD_WIDTH - (axisRect.right()/xCoeff + 0.5), MD_HEIGHT - (axisRect.bottom()/yCoeff + 0.5));
    return QRect(topLeft, bottomRight).normalized();
}

QRect QnPlAxisResource::gridRectToAxisRect(const QRect& gridRect)
{
    qreal xCoeff = 9999.0 / MD_WIDTH;
    qreal yCoeff = 9999.0 / MD_HEIGHT;
    QPoint topLeft(9999.0 - (gridRect.left()*xCoeff + 0.5), 9999.0 - (gridRect.top()*yCoeff + 0.5));
    QPoint bottomRight(9999.0 - ((gridRect.right()+1)*xCoeff + 0.5), 9999.0 - ((gridRect.bottom()+1)*yCoeff + 0.5));
    return QRect(topLeft, bottomRight).normalized();
}

bool QnPlAxisResource::readMotionInfo()
{
    
    {
        QMutexLocker lock(&m_mutex);
        qint64 time = qnSyncTime->currentMSecsSinceEpoch();
        if (time - m_lastMotionReadTime < qint64(MOTION_INFO_UPDATE_INTERVAL))
            return true;

        m_lastMotionReadTime = time;
    }


    

    // read motion windows coordinates
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=list&group=Motion"));
    if (status != CL_HTTP_SUCCESS) {
        if (status == CL_HTTP_AUTH_REQUIRED)
            setStatus(Qn::Unauthorized);
        return false;
    }
    QByteArray body;
    http.readAll(body);
    QList<QByteArray> lines = body.split('\n');
    QMap<int,WindowInfo> windows;

    

    for (int i = 0; i < lines.size(); ++i)
    {
        QList<QByteArray> params = lines[i].split('.');
        if (params.size() < 2)
            continue;
        int windowNum = params[params.size()-2].mid(1).toInt();
        QList<QByteArray> values = params[params.size()-1].split('=');
        if (values.size() < 2)
            continue;
        if (values[0] == "Left")
            windows[windowNum].left = values[1].toInt();
        else if (values[0] == "Right")
            windows[windowNum].right = values[1].toInt();
        else if (values[0] == "Top")
            windows[windowNum].top = values[1].toInt();
        else if (values[0] == "Bottom")
            windows[windowNum].bottom = values[1].toInt();
        if (values[0] == "WindowType")
        {
            if (values[1] == "include")
                windows[windowNum].rectType = WindowInfo::Motion;
            else
                windows[windowNum].rectType = WindowInfo::MotionMask;
        }
    }


    QMutexLocker lock(&m_mutex);

    for (QMap<int,WindowInfo>::const_iterator itr = windows.begin(); itr != windows.end(); ++itr)
    {
        if (itr.value().isValid())
        {
            if (itr.value().rectType == WindowInfo::Motion)
                m_motionWindows[itr.key()] = axisRectToGridRect(itr.value().toRect());
            else
                m_motionMask[itr.key()] = axisRectToGridRect(itr.value().toRect());
        }
    }

    return true;
}

bool resolutionGreatThan(const QnPlAxisResource::AxisResolution& res1, const QnPlAxisResource::AxisResolution& res2)
{
    int square1 = res1.size.width() * res1.size.height();
    int square2 = res2.size.width() * res2.size.height();

    return !(square1 < square2);
}

CameraDiagnostics::Result QnPlAxisResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();

    //TODO #ak check firmware version. it must be >= 5.0.0 to support I/O ports
    {
        CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
        CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=list&group=root.Properties.Firmware.Version"));
        if (status == CL_HTTP_SUCCESS) {
            QByteArray firmware;
            http.readAll(firmware);
            firmware = firmware.mid(firmware.indexOf('=')+1);
            setFirmware(QString::fromUtf8(firmware));
        }
    }

    {
        // enable send motion into H.264 stream
        CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
        //CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.I0.MPEG.UserDataEnabled=yes")); 
        CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.TriggerDataEnabled=yes&Audio.A0.Enabled=").append(isAudioEnabled() ? "yes" : "no"));
        //CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.I0.MPEG.UserDataEnabled=yes&Image.I1.MPEG.UserDataEnabled=yes&Image.I2.MPEG.UserDataEnabled=yes&Image.I3.MPEG.UserDataEnabled=yes"));
        if (status != CL_HTTP_SUCCESS) {
            if (status == CL_HTTP_AUTH_REQUIRED)
                setStatus(Qn::Unauthorized);
            return CameraDiagnostics::UnknownErrorResult();
        }
    }

    {
        //reading RTSP port
        CLSimpleHTTPClient http( getHostAddress(), QUrl( getUrl() ).port( DEFAULT_AXIS_API_PORT ), getNetworkTimeout(), getAuth() );
        CLHttpStatus status = http.doGET( QByteArray( "axis-cgi/param.cgi?action=list&group=Network.RTSP.Port" ) );
        if( status != CL_HTTP_SUCCESS )
        {
            if( status == CL_HTTP_AUTH_REQUIRED )
                setStatus( Qn::Unauthorized );
            return CameraDiagnostics::UnknownErrorResult();
        }
        QByteArray paramStr;
        http.readAll( paramStr );
        bool ok = false;
        const int rtspPort = paramStr.trimmed().mid( paramStr.indexOf( '=' ) + 1 ).toInt( &ok );
        if( ok )
            setMediaPort( rtspPort );
    }

    readMotionInfo();

    // determin camera max resolution
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=list&group=Properties.Image.Resolution"));
    if (status != CL_HTTP_SUCCESS) {
        if (status == CL_HTTP_AUTH_REQUIRED)
            setStatus(Qn::Unauthorized);
        return CameraDiagnostics::UnknownErrorResult();
    }

    
   
        
    QByteArray body;
    http.readAll(body);

    {
        QMutexLocker lock(&m_mutex);

        m_resolutionList.clear();
        int paramValuePos = body.indexOf('=');
        if (paramValuePos == -1)
        {
            qWarning() << Q_FUNC_INFO << "Unexpected server answer. Can't read resolution list";
            return CameraDiagnostics::UnknownErrorResult();
        }

        QList<QByteArray> rawResolutionList = body.mid(paramValuePos+1).split(',');
        for (int i = 0; i < rawResolutionList.size(); ++i)
        {
            QByteArray resolutionStr = rawResolutionList[i].toLower().trimmed();

            if (resolutionStr == "qcif")
            {
                m_resolutionList << AxisResolution(QSize(176,144), resolutionStr);
            }

            else if (resolutionStr == "cif")
            {
                m_resolutionList << AxisResolution(QSize(352,288), resolutionStr);
            }

            else if (resolutionStr == "2cif")
            {
                m_resolutionList << AxisResolution(QSize(704,288), resolutionStr);
            }

            else if (resolutionStr == "4cif")
            {
                m_resolutionList << AxisResolution(QSize(704,576), resolutionStr);
            }

            else if (resolutionStr == "d1")
            {
                m_resolutionList << AxisResolution(QSize(720,576), resolutionStr);
            }
            else {
                QList<QByteArray> dimensions = resolutionStr.split('x');
                if (dimensions.size() >= 2) {
                    QSize size( dimensions[0].trimmed().toInt(), dimensions[1].trimmed().toInt());
                    m_resolutionList << AxisResolution(size, resolutionStr);
                }
            }
        }
    }   //releasing mutex so that not to make other threads using the resource to wait for completion of heavy-wait io & pts initialization,
            //m_initMutex is locked up the stack
    
    qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreatThan);

    //detecting primary & secondary resolution
    m_resolutions[PRIMARY_ENCODER_INDEX] = getMaxResolution();
    m_resolutions[SECONDARY_ENCODER_INDEX] = getNearestResolution(
        QSize(480,316),
        getResolutionAspectRatio(getMaxResolution()) );
    if (m_resolutions[SECONDARY_ENCODER_INDEX].size.isEmpty())
        m_resolutions[SECONDARY_ENCODER_INDEX] = getNearestResolution(QSize(480,316), 0.0); // try to get secondary resolution again (ignore aspect ratio)

    //detecting and saving selected resolutions
    CameraMediaStreams mediaStreams;
    mediaStreams.streams.push_back( CameraMediaStreamInfo( m_resolutions[PRIMARY_ENCODER_INDEX].size, CODEC_ID_H264 ) );
    if( !m_resolutions[SECONDARY_ENCODER_INDEX].size.isEmpty() )
        mediaStreams.streams.push_back( CameraMediaStreamInfo( m_resolutions[SECONDARY_ENCODER_INDEX].size, CODEC_ID_H264 ) );
    saveResolutionList( mediaStreams );

    //root.Image.MotionDetection=no
    //root.Image.I0.TriggerData.MotionDetectionEnabled=yes
    //root.Image.I1.TriggerData.MotionDetectionEnabled=yes
    //root.Properties.Motion.MaxNbrOfWindows=10

    initializeIOPorts( &http );

    /* Ptz capabilities will be initialized by PTZ controller pool. */

    // determin camera max resolution

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

QnPlAxisResource::AxisResolution QnPlAxisResource::getMaxResolution() const
{
    QMutexLocker lock(&m_mutex);
    return !m_resolutionList.isEmpty() ? m_resolutionList[0] : AxisResolution();
}

float QnPlAxisResource::getResolutionAspectRatio(const AxisResolution& resolution) const
{
    if (!resolution.size.isEmpty())
        return resolution.size.width() / (float) resolution.size.height();
    else
        return 1.0;
}


QnPlAxisResource::AxisResolution QnPlAxisResource::getNearestResolution(const QSize& resolution, float aspectRatio) const
{
    QMutexLocker lock(&m_mutex);

    float requestSquare = resolution.width() * resolution.height();
    int bestIndex = -1;
    float bestMatchCoeff = (float)INT_MAX;
    for (int i = 0; i < m_resolutionList.size(); ++ i)
    {
        float ar = getResolutionAspectRatio(m_resolutionList[i]);
        if (aspectRatio != 0 && qAbs(ar-aspectRatio) > MAX_AR_EPS)
            continue;
        float square = m_resolutionList[i].size.width() * m_resolutionList[i].size.height();
        float matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff < bestMatchCoeff)
        {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
        }
    }
    return bestIndex >= 0 ? m_resolutionList[bestIndex] : AxisResolution();
}

QRect QnPlAxisResource::getMotionWindow(int num) const
{
    QMutexLocker lock(&m_mutex);
    return m_motionWindows.value(num);
}

QMap<int, QRect> QnPlAxisResource::getMotionWindows() const
{
    return m_motionWindows;
}


bool QnPlAxisResource::removeMotionWindow(int wndNum)
{
    //QMutexLocker lock(&m_mutex);

    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QString(QLatin1String("axis-cgi/param.cgi?action=remove&group=Motion.M%1")).arg(wndNum));
    return status == CL_HTTP_SUCCESS;
}

int QnPlAxisResource::addMotionWindow()
{
    //QMutexLocker lock(&m_mutex);

    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QLatin1String("axis-cgi/param.cgi?action=add&group=Motion&template=motion&Motion.M.WindowType=include&Motion.M.ImageSource=0"));
    if (status != CL_HTTP_SUCCESS)
        return -1;
    QByteArray data;
    http.readAll(data);
    data = data.split(' ')[0].mid(1);
    return data.toInt();
}

bool QnPlAxisResource::updateMotionWindow(int wndNum, int sensitivity, const QRect& rect)
{
    //QMutexLocker lock(&m_mutex);

    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QString(QLatin1String("axis-cgi/param.cgi?action=update&group=Motion&\
Motion.M%1.Name=HDWitnessWindow%1&Motion.M%1.ImageSource=0&Motion.M%1.WindowType=include&\
Motion.M%1.Left=%2&Motion.M%1.Right=%3&Motion.M%1.Top=%4&Motion.M%1.Bottom=%5&Motion.M%1.Sensitivity=%6"))
.arg(wndNum).arg(rect.left()).arg(rect.right()).arg(rect.top()).arg(rect.bottom()).arg(sensitivity));
return status == CL_HTTP_SUCCESS;
}

int QnPlAxisResource::toAxisMotionSensitivity(int sensitivity)
{
    static const int sens[10] =
    {
        0,
        10,
        25,
        40,
        50,
        60,
        70,
        80,
        90,
        100,
    };

    return sens[sensitivity];
}

void QnPlAxisResource::setMotionMaskPhysical(int /*channel*/)
{
    m_mutex.lock();

    bool readMotion = (m_lastMotionReadTime == 0);

    m_mutex.unlock();

    if (readMotion)
        readMotionInfo();


    m_mutex.lock();

    const QnMotionRegion region = getMotionRegion(0);

    QMap<int, QRect> existsWnd = m_motionWindows; // the key is window number
    QMultiMap<int, QRect> newWnd = region.getAllMotionRects(); // the key is motion sensitivity

    while (existsWnd.size() > newWnd.size())
    {
        int key = (existsWnd.end()-1).key();
        removeMotionWindow(key);
        existsWnd.remove(key);
    }
    while (existsWnd.size() < newWnd.size()) {
        int newNum = addMotionWindow();
        existsWnd.insert(newNum, QRect());
    }

    QMap<int, QRect>::iterator cameraWndItr = existsWnd.begin();
    QMap<int, QRect>::iterator motionWndItr = newWnd.begin();
    m_motionWindows.clear();

    m_mutex.unlock();

    while (cameraWndItr != existsWnd.end())
    {
        QRect axisRect = gridRectToAxisRect(motionWndItr.value());
        updateMotionWindow(cameraWndItr.key(), toAxisMotionSensitivity(motionWndItr.key()), axisRect);

        m_mutex.lock();

        m_motionWindows.insert(cameraWndItr.key(), motionWndItr.value());

        m_mutex.unlock();

        *cameraWndItr = axisRect;
        ++cameraWndItr;
        ++motionWndItr;
    }
}

QnConstResourceAudioLayoutPtr QnPlAxisResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    if (isAudioEnabled()) {
        const QnAxisStreamReader* axisReader = dynamic_cast<const QnAxisStreamReader*>(dataProvider);
        if (axisReader && axisReader->getDPAudioLayout())
            return axisReader->getDPAudioLayout();
        else
            return QnPhysicalCameraResource::getAudioLayout(dataProvider);
    }
    else
        return QnPhysicalCameraResource::getAudioLayout(dataProvider);
}

int QnPlAxisResource::getChannelNumAxis() const
{
    QString phId = getPhysicalId();

    int index = phId.indexOf(QLatin1String("_channel_"));
    if (index<0)
        return 1;

    index += 9;

    if (index >= phId.length() )
        return 1;

    int result = phId.mid(index).toInt();

    return result;
}

        // TEMPLATE STRUCT select1st
template<class _Pair>
    struct select1st
        : public std::unary_function<_Pair, typename _Pair::first_type>
    {    // functor for unary first of pair selector operator
    const typename _Pair::first_type& operator()(const _Pair& _Left) const
        {    // apply first selector operator to pair operand
        return (_Left.first);
        }
    };

//!Implementation of QnSecurityCamResource::getRelayOutputList
QStringList QnPlAxisResource::getRelayOutputList() const
{
    QStringList idList;
    std::transform(
        m_outputPortNameToIndex.begin(),
        m_outputPortNameToIndex.end(),
        std::back_inserter(idList),
        select1st<std::map<QString, unsigned int>::value_type>() );
    return idList;
}

QStringList QnPlAxisResource::getInputPortList() const
{
    QStringList idList;
    std::transform(
        m_inputPortNameToIndex.begin(),
        m_inputPortNameToIndex.end(),
        std::back_inserter(idList),
        select1st<std::map<QString, unsigned int>::value_type>() );
    return idList;
}

//!Implementation of QnSecurityCamResource::setRelayOutputState
bool QnPlAxisResource::setRelayOutputState(
    const QString& outputID,
    bool activate,
    unsigned int autoResetTimeoutMS )
{
    std::map<QString, unsigned int>::const_iterator it = outputID.isEmpty()
        ? m_outputPortNameToIndex.begin()
        : m_outputPortNameToIndex.find( outputID );
    if( it == m_outputPortNameToIndex.end() )
        return false;

    QString cmd = lit("axis-cgi/io/port.cgi?action=%1:%2").arg(it->second+1).arg(QLatin1String(activate ? "/" : "\\"));
    if( autoResetTimeoutMS > 0 )
    {
        //adding auto-reset
        cmd += QString::number(autoResetTimeoutMS)+QLatin1String(activate ? "\\" : "/");
    }

    CLSimpleHTTPClient httpClient( getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth() );

    //cmd = QLatin1String("/axis-cgi/param.cgi?action=list&group=IOPort.I0.Configurable");
    //cmd = QLatin1String("/axis-cgi/param.cgi?action=list&group=IOPort.I1.Output.Name");
    //cmd = QLatin1String("/axis-cgi/io/port.cgi?check=1,2");
    //cmd = QLatin1String("/axis-cgi/io/port.cgi?checkactive=1,2");
    //cmd = QLatin1String("/axis-cgi/io/port.cgi?action=2:/300\\500/300\\");
    //cmd = QLatin1String("/axis-cgi/param.cgi?action=list&group=IOPort.I1");

    CLHttpStatus status = httpClient.doGET( cmd );
    if( status / 100 != 2 )
    {
        NX_LOG( lit("Failed to set camera %1 port %2 output state to %3. Result: %4").
            arg(getHostAddress()).arg(it->first).arg(activate).arg(::toString(status)), cl_logWARNING );
        return false;
    }

    return true;
}

CLHttpStatus QnPlAxisResource::readAxisParameter(
    CLSimpleHTTPClient* const httpClient,
    const QString& paramName,
    QVariant* paramValue )
{
    CLHttpStatus status = httpClient->doGET( lit("axis-cgi/param.cgi?action=list&group=%1").arg(paramName).toLatin1() );
    if( status == CL_HTTP_SUCCESS )
    {
        QByteArray body;
        httpClient->readAll( body );
        const QStringList& paramItems = QString::fromLatin1(body.data()).split(QLatin1Char('='));
        if( paramItems.size() == 2 && paramItems[0] == paramName )
        {
            *paramValue = paramItems[1];
            return CL_HTTP_SUCCESS;
        }
        else
        {
            NX_LOG( lit("Failed to read param %1 of camera %2. Unexpected response: %3").
                arg(paramName).arg(getHostAddress()).arg(QLatin1String(body)), cl_logWARNING );
            return CL_HTTP_BAD_REQUEST;
        }
    }
    else
    {
        NX_LOG( lit("Failed to param %1 of camera %2. Result: %3").
            arg(paramName).arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
        return status;
    }
}

CLHttpStatus QnPlAxisResource::readAxisParameter(
    CLSimpleHTTPClient* const httpClient,
    const QString& paramName,
    QString* paramValue )
{
    QVariant val;
    CLHttpStatus status = readAxisParameter( httpClient, paramName, &val );
    *paramValue = val.toString().trimmed();
    return status;
}

CLHttpStatus QnPlAxisResource::readAxisParameter(
    CLSimpleHTTPClient* const httpClient,
    const QString& paramName,
    unsigned int* paramValue )
{
    QVariant val;
    CLHttpStatus status = readAxisParameter( httpClient, paramName, &val );
    *paramValue = val.toUInt();
    return status;
}

void QnPlAxisResource::onMonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient )
{
    Q_ASSERT( httpClient );

    if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        NX_LOG( lit("Axis camera %1. Failed to subscribe to input port(s) monitoring. %2").
            arg(getUrl()).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );

        QMutexLocker lk( &m_inputPortMutex );
        if( m_inputPortHttpMonitor == httpClient )
            m_inputPortHttpMonitor.reset();
        return;
    }

    //analyzing response headers (if needed)
    if( !m_multipartContentParser.setContentType(httpClient->contentType()) )
    {
        static const char* multipartContentType = "multipart/x-mixed-replace";

        //unexpected content type
        NX_LOG( lit("Error monitoring input port(s) on Axis camera %1. Unexpected Content-Type (%2) in monitor response. Expected: %3").
            arg(getUrl()).arg(QLatin1String(httpClient->contentType())).arg(QLatin1String(multipartContentType)), cl_logWARNING );

        QMutexLocker lk( &m_inputPortMutex );
        if( m_inputPortHttpMonitor == httpClient )
            m_inputPortHttpMonitor.reset();
        return;
    }
}

void QnPlAxisResource::onMonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient )
{
    Q_ASSERT( httpClient );

    const nx_http::BufferType& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
    for( int offset = 0; offset < msgBodyBuf.size(); )
    {
        size_t bytesProcessed = 0;
        nx_http::MultipartContentParserHelper::ResultCode resultCode = m_multipartContentParser.parseBytes(
            nx_http::ConstBufferRefType(msgBodyBuf, offset),
            &bytesProcessed );
        offset += (int) bytesProcessed;
        switch( resultCode )
        {
            case nx_http::MultipartContentParserHelper::partDataDone:
                if( m_currentMonitorData.isEmpty() )
                {
                    if( !m_multipartContentParser.prevFoundData().isEmpty() )
                        notificationReceived( m_multipartContentParser.prevFoundData() );
                }
                else
                {
                    if( !m_multipartContentParser.prevFoundData().isEmpty() )
                        m_currentMonitorData.append(
                            m_multipartContentParser.prevFoundData().data(),
                            (int) m_multipartContentParser.prevFoundData().size() );
                    notificationReceived( m_currentMonitorData );
                    m_currentMonitorData.clear();
                }
                break;

            case nx_http::MultipartContentParserHelper::someDataAvailable:
                if( !m_multipartContentParser.prevFoundData().isEmpty() )
                    m_currentMonitorData.append(
                        m_multipartContentParser.prevFoundData().data(),
                        (int) m_multipartContentParser.prevFoundData().size() );
                break;

            case nx_http::MultipartContentParserHelper::eof:
                //TODO/IMPL reconnect
                break;

            default:
                continue;
        }
    }
}

void QnPlAxisResource::onMonitorConnectionClosed( nx_http::AsyncHttpClientPtr /*httpClient*/ )
{
    //TODO/IMPL reconnect
}

void QnPlAxisResource::initializeIOPorts( CLSimpleHTTPClient* const http )
{
    unsigned int inputPortCount = 0;
    CLHttpStatus status = readAxisParameter( http, QLatin1String("Input.NbrOfInputs"), &inputPortCount );
    if( status != CL_HTTP_SUCCESS )
    {
        NX_LOG( lit("Failed to read number of input ports of camera %1. Result: %2").
            arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
    }
    else if( inputPortCount > 0 )
    {
        setCameraCapability(Qn::RelayInputCapability, true);
    }

    unsigned int outputPortCount = 0;
    status = readAxisParameter( http, QLatin1String("Output.NbrOfOutputs"), &outputPortCount );
    if( status != CL_HTTP_SUCCESS )
    {
        NX_LOG( lit("Failed to read number of output ports of camera %1. Result: %2").
            arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
    }
    else if( outputPortCount > 0 )
    {
        setCameraCapability(Qn::RelayOutputCapability, true);
    }

    //reading port direction and names
    for( unsigned int i = 0; i < inputPortCount+outputPortCount; ++i )
    {
        QString portDirection;
        status = readAxisParameter( http, lit("IOPort.I%1.Direction").arg(i), &portDirection );
        if( status != CL_HTTP_SUCCESS )
        {
            NX_LOG( lit("Failed to read name of port %1 of camera %2. Result: %3").
                arg(i).arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
            continue;
        }

        QString portName;
        status = readAxisParameter( http, lit("IOPort.I%1.%2.Name").arg(i).arg(portDirection), &portName );
        if( status != CL_HTTP_SUCCESS )
        {
            NX_LOG( lit("Failed to read name of input port %1 of camera %2. Result: %3").
                arg(i).arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
            continue;
        }

        if( portDirection == QLatin1String("input") )
            m_inputPortNameToIndex[portName] = i;
        else if( portDirection == QLatin1String("output") )
            m_outputPortNameToIndex[portName] = i;
    }

    //TODO/IMPL periodically update port names in case some one changes it via camera's webpage
    //startInputPortMonitoring();
}

void QnPlAxisResource::notificationReceived( const nx_http::ConstBufferRefType& notification )
{
    //1I:H, 1I:L, 1I:/, "1I:\"
    if( notification.isEmpty() )
        return;
    NX_LOG( lit("Received notification %1 from %2").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logDEBUG1 );

    //notification
    size_t sepPos = nx_http::find_first_of( notification, ":" );
    if( sepPos == nx_http::BufferNpos || sepPos+1 >= notification.size() )
    {
        NX_LOG( lit("Error parsing notification %1 from %2. Event type not found").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logINFO );
        return;
    }
    const char eventType = notification[sepPos+1];
    size_t portTypePos = nx_http::find_first_not_of( notification, "0123456789" );
    if( portTypePos == nx_http::BufferNpos )
    {
        NX_LOG( lit("Error parsing notification %1 from %2. Port type not found").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logINFO );
        return;
    }
    const unsigned int portNumber = notification.mid( 0, portTypePos ).toUInt();
    const char portType = notification[portTypePos];
    NX_LOG( lit("%1 port %2 changed its state to %3. Camera %4").
        arg(QLatin1String(portType == 'I' ? "Input" : "Output")).arg(portNumber).arg(QLatin1String(eventType == '/' ? "active" : "inactive")).arg(getUrl()), cl_logDEBUG1 );

    if( portType != 'I' )
        return;

    switch( eventType )
    {
        case '/':
        case '\\':
            emit cameraInput(
                toSharedPointer(),
                QString::number(portNumber),
                eventType == '/' ? true : false,
                QDateTime::currentMSecsSinceEpoch() );
            break;

        default:
            break;
    }
}

QnAbstractPtzController *QnPlAxisResource::createPtzControllerInternal() {
    return new QnAxisPtzController(toSharedPointer(this));
}

QnPlAxisResource::AxisResolution QnPlAxisResource::getResolution( int encoderIndex ) const
{
    return (unsigned int)encoderIndex < sizeof(m_resolutions)/sizeof(*m_resolutions)
        ? m_resolutions[encoderIndex]
        : QnPlAxisResource::AxisResolution();
}

int QnPlAxisResource::getChannel() const
{
    return getChannelNumAxis() - 1;
}

#endif // #ifdef ENABLE_AXIS
