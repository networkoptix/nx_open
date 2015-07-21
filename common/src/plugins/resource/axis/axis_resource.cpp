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
#include "api/model/api_ioport_data.h"
#include "utils/serialization/json.h"
#include <utils/common/model_functions.h>
#include "utils/common/concurrent.h"
#include "common/common_module.h"

using namespace std;

const QString QnPlAxisResource::MANUFACTURE(lit("Axis"));
static const float MAX_AR_EPS = 0.04f;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
static const quint16 DEFAULT_AXIS_API_PORT = 80;
static const int AXIS_IO_KEEP_ALIVE_TIME = 1000 * 15;

int QnPlAxisResource::portIdToIndex(const QString& id) const
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_ioPortIdList.size(); ++i) {
        if (m_ioPortIdList[i] == id)
            return i;
    }
    return id.mid(1).toInt();
}

QString QnPlAxisResource::portIndexToId(int num) const
{
    QMutexLocker lock(&m_mutex);
    if (num < m_ioPortIdList.size())
        return m_ioPortIdList[num];
    return QString(lit("I")) + QString::number(num);
}

int QnPlAxisResource::portDisplayNameToIndex(const QString& id) const
{
    QString cleanNum;
    for (int i = 0; i < id.size(); ++i) {
        if (id.at(i) >= L'0' && id.at(i) <= L'9')
            cleanNum += id.at(i);
    }
    int num = cleanNum.toInt();
    if (!id.contains(lit("IO")))
        num--; // if we got string like IO<xxx> its based from zerro, otherwise its based from 1
    return num;
}

QString QnPlAxisResource::portIndexToReqParam(int number) const
{
    return QString::number(number + 1);
}

class AxisIOMessageBodyParser: public AbstractByteStreamFilter
{
public:
    AxisIOMessageBodyParser(QnPlAxisResource* owner): m_owner(owner) {}

    virtual bool processData( const QnByteArrayConstRef& notification ) override
    {
        m_owner->notificationReceived(notification);
        return true;
    }
private:
    QnPlAxisResource* m_owner;
};

QnPlAxisResource::QnPlAxisResource()
{
    setVendor(lit("Axis"));
    setDefaultAuth(QLatin1String("root"), QLatin1String("root"));
    m_lastMotionReadTime = 0;
    connect( this, &QnResource::propertyChanged, this, &QnPlAxisResource::at_propertyChanged, Qt::DirectConnection );
}

QnPlAxisResource::~QnPlAxisResource()
{
    stopInputPortMonitoringAsync();
}

bool QnPlAxisResource::checkIfOnlineAsync( std::function<void(bool)>&& completionHandler )
{
    QUrl apiUrl;
    apiUrl.setScheme( lit("http") );
    apiUrl.setHost( getHostAddress() );
    apiUrl.setPort( QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT) );
    apiUrl.setUserName( getAuth().user() );
    apiUrl.setPassword( getAuth().password() );
    apiUrl.setPath( lit("/axis-cgi/param.cgi") );
    apiUrl.setQuery( lit("action=list&group=root.Network.eth0.MACAddress") );

    QString resourceMac = getMAC().toString();
    auto requestCompletionFunc = [resourceMac, completionHandler]
        ( SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody ) mutable
    {
        if( osErrorCode != SystemError::noError ||
            statusCode != nx_http::StatusCode::ok )
        {
            return completionHandler( false );
        }

        int sepIndex = msgBody.indexOf('=');
        if( sepIndex == -1 )
            return completionHandler( false );
        QByteArray macAddress = msgBody.mid(sepIndex+1).trimmed();
        macAddress.replace( ':', '-' );
        completionHandler( macAddress == resourceMac.toLatin1() );
    };

    return nx_http::downloadFileAsync(
        apiUrl,
        requestCompletionFunc );
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

bool QnPlAxisResource::startInputPortMonitoringAsync( std::function<void(bool)>&& /*completionHandler*/ )
{
    {
        QMutexLocker lock(&m_mutex);
        m_ioStates.clear();
    }

    bool rez = startIOMonitor(Qn::PT_Input, m_ioHttpMonitor[0]);
    if (hasCameraCapabilities(Qn::IOModuleCapability))
        startIOMonitor(Qn::PT_Output, m_ioHttpMonitor[1]);

    QMutexLocker lk( &m_inputPortMutex );
    readCurrentIOStateAsync(); // read current state after starting IO monitor to avoid time period if port updated before monitor was running
    return rez;
}

bool QnPlAxisResource::startIOMonitor(Qn::IOPortType portType, IOMonitor& ioMonitor)
{
    if( hasFlags(Qn::foreigner) ||      //we do not own camera
        m_ioPorts.empty() )    //camera report no io 
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

    QString portList;
    for (const auto& port: getIOPorts()) {
        //if (port.portType != Qn::PT_Disabled) {
        if (port.portType == portType) {
            if (!portList.isEmpty())
                portList += lit(",");
            portList += portIndexToReqParam(portIdToIndex(port.id));
        }
    }
    if (portList.isEmpty())
        return false; // no ports to listen

    requestPath += portList;
    requestUrl.setPath( requestPath );

    QnMutexLocker lk( &m_inputPortMutex );
    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    connect( httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnPlAxisResource::onMonitorResponseReceived, Qt::DirectConnection );
    connect( httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable, this, &QnPlAxisResource::onMonitorMessageBodyAvailable, Qt::DirectConnection );
    connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnPlAxisResource::onMonitorConnectionClosed, Qt::DirectConnection );
    httpClient->setTotalReconnectTries( nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
    httpClient->setUserName( auth.user() );
    httpClient->setUserPassword( auth.password() );
    httpClient->setMessageBodyReadTimeoutMs(AXIS_IO_KEEP_ALIVE_TIME * 2);

    ioMonitor.contentParser = std::make_shared<nx_http::MultipartContentParser>();
    ioMonitor.contentParser->setNextFilter(std::make_shared<AxisIOMessageBodyParser>(this));

    if( !httpClient->doGet( requestUrl ) )
        return false;
    ioMonitor.httpClient = httpClient;
    return true;
}

void QnPlAxisResource::stopInputPortMonitoringAsync()
{
    resetHttpClient(m_ioHttpMonitor[0].httpClient);
    resetHttpClient(m_ioHttpMonitor[1].httpClient);
    resetHttpClient(m_inputPortStateReader);
}

void QnPlAxisResource::resetHttpClient(nx_http::AsyncHttpClientPtr& value)
{
    QnMutexLocker lk( &m_inputPortMutex );

    if( !value )
        return;

    nx_http::AsyncHttpClientPtr httpClient;
    httpClient.swap(value);

    lk.unlock();
    httpClient->terminate();
    httpClient.reset();
    lk.relock();
}

bool QnPlAxisResource::isInputPortMonitored() const
{
    QnMutexLocker lk( &m_inputPortMutex );
    return m_ioHttpMonitor[0].httpClient.get() != nullptr;
}

bool QnPlAxisResource::isInitialized() const
{
    QnMutexLocker lock( &m_mutex );
    return hasCameraCapabilities(Qn::IOModuleCapability) ? base_type::isInitialized() : !m_resolutionList.isEmpty();
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
        QnMutexLocker lock( &m_mutex );
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


    QnMutexLocker lock( &m_mutex );

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

    if (hasVideo(0))
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
    if (hasVideo(0))
    {
        QnMutexLocker lock( &m_mutex );

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
    if (hasVideo(0)) 
    {
        qSort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreatThan);

        //detecting primary & secondary resolution
        m_resolutions[PRIMARY_ENCODER_INDEX] = getMaxResolution();
        m_resolutions[SECONDARY_ENCODER_INDEX] = getNearestResolution(
            QSize(480,316),
            getResolutionAspectRatio(getMaxResolution()) );
        if (m_resolutions[SECONDARY_ENCODER_INDEX].size.isEmpty())
            m_resolutions[SECONDARY_ENCODER_INDEX] = getNearestResolution(QSize(480,316), 0.0); // try to get secondary resolution again (ignore aspect ratio)
    }
    //root.Image.MotionDetection=no
    //root.Image.I0.TriggerData.MotionDetectionEnabled=yes
    //root.Image.I1.TriggerData.MotionDetectionEnabled=yes
    //root.Properties.Motion.MaxNbrOfWindows=10

    if (!initializeIOPorts( &http ))
        return CameraDiagnostics::CameraInvalidParams(tr("Can't initialize IO port settings"));

    /* Ptz capabilities will be initialized by PTZ controller pool. */

    // determin camera max resolution

    saveParams();

    return CameraDiagnostics::NoErrorResult();
}

QnPlAxisResource::AxisResolution QnPlAxisResource::getMaxResolution() const
{
    QnMutexLocker lock( &m_mutex );
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
    QnMutexLocker lock( &m_mutex );

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
    QnMutexLocker lock( &m_mutex );
    return m_motionWindows.value(num);
}

QMap<int, QRect> QnPlAxisResource::getMotionWindows() const
{
    return m_motionWindows;
}


bool QnPlAxisResource::removeMotionWindow(int wndNum)
{
    //QnMutexLocker lock( &m_mutex );

    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QString(QLatin1String("axis-cgi/param.cgi?action=remove&group=Motion.M%1")).arg(wndNum));
    return status == CL_HTTP_SUCCESS;
}

int QnPlAxisResource::addMotionWindow()
{
    //QnMutexLocker lock( &m_mutex );

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
    //QnMutexLocker lock( &m_mutex );

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

//!Implementation of QnSecurityCamResource::setRelayOutputState
bool QnPlAxisResource::setRelayOutputState(
    const QString& outputID,
    bool activate,
    unsigned int autoResetTimeoutMS )
{
    int portNum = 0;
    if (outputID.isEmpty()) {
        QMutexLocker lock(&m_mutex);
        for (const auto& value: m_ioPorts) {
            if (value.portType == Qn::PT_Output)
                portNum = portIdToIndex(value.id);
        }
    }
    else {
        portNum = portIdToIndex(outputID);
    }
    QString cmd = lit("axis-cgi/io/port.cgi?action=%1:%2").arg(portIndexToReqParam(portNum)).arg(QLatin1String(activate ? "/" : "\\"));
    if( autoResetTimeoutMS > 0 )
    {
        //adding auto-reset
        cmd += QString::number(autoResetTimeoutMS)+QLatin1String(activate ? "\\" : "");
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
            arg(getHostAddress()).arg(outputID).arg(activate).arg(::toString(status)), cl_logWARNING );
        return false;
    }

    return true;
}

CLHttpStatus QnPlAxisResource::readAxisParameters(
    const QString& rootPath,
    CLSimpleHTTPClient* const httpClient,
    QList<QPair<QByteArray,QByteArray>>& params)
{
    params.clear();
    CLHttpStatus status = httpClient->doGET( lit("axis-cgi/param.cgi?action=list&group=%1").arg(rootPath).toLatin1() );
    if( status == CL_HTTP_SUCCESS )
    {
        QByteArray body;
        httpClient->readAll( body );
        for (const QByteArray& line: body.split('\n'))
        {
            const auto& paramItems = line.split('=');
            if( paramItems.size() == 2)
                params << QPair<QByteArray, QByteArray>(paramItems[0], paramItems[1]);
        }
    }
    else
    {
        NX_LOG( lit("Failed to read params from path %1 of camera %2. Result: %3").
            arg(rootPath).arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
    }
    return status;
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
    int index = -1;
    QMutexLocker lk( &m_inputPortMutex );
    if( m_ioHttpMonitor[0].httpClient == httpClient )
        index = 0;
    else if( m_ioHttpMonitor[1].httpClient == httpClient )
        index = 1;
    else
        return;

    if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        NX_LOG( lit("Axis camera %1. Failed to subscribe to input port(s) monitoring. %2").
            arg(getUrl()).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );
        return;
    }

    //analyzing response headers (if needed)
    if( !m_ioHttpMonitor[index].contentParser->setContentType(httpClient->contentType()) )
    {
        static const char* multipartContentType = "multipart/x-mixed-replace";

        //unexpected content type
        NX_LOG( lit("Error monitoring input port(s) on Axis camera %1. Unexpected Content-Type (%2) in monitor response. Expected: %3").
            arg(getUrl()).arg(QLatin1String(httpClient->contentType())).arg(QLatin1String(multipartContentType)), cl_logWARNING );
        return;
    }
}

void QnPlAxisResource::onCurrentIOStateResponseReceived( nx_http::AsyncHttpClientPtr httpClient )
{
    Q_ASSERT( httpClient );

    if (httpClient->failed()) 
    {
        NX_LOG( lit("Axis camera %1. Failed to read current IO state. No HTTP response").arg(getUrl()), cl_logWARNING );
    }
    else if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        NX_LOG( lit("Axis camera %1. Failed to read current IO state. %2").
            arg(getUrl()).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );
    }
    else {
        QList<QByteArray> result = httpClient->fetchMessageBodyBuffer().split('\n');
        for (const auto& line: result) {
            QList<QByteArray> params = line.trimmed().split('=');
            if (params.size() == 2) {
                int portIndex = portDisplayNameToIndex(QString::fromLatin1(params[0]));
                QString portId = portIndexToId(portIndex);
                qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
                updateIOState(portId, params[1] == "active", timestamp, false);
            }
        }
    }

    QMutexLocker lk( &m_inputPortMutex );
    if( m_inputPortStateReader == httpClient )
        m_inputPortStateReader.reset();
}

void QnPlAxisResource::onMonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient )
{
    Q_ASSERT( httpClient );

    int index = -1;
    QMutexLocker lk( &m_inputPortMutex );
    if( m_ioHttpMonitor[0].httpClient == httpClient )
        index = 0;
    else if( m_ioHttpMonitor[1].httpClient == httpClient )
        index = 1;
    else
        return;

    const nx_http::BufferType& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
    auto parser = m_ioHttpMonitor[index].contentParser;
    lk.unlock();
    parser->processData(msgBodyBuf);
}

void QnPlAxisResource::onMonitorConnectionClosed( nx_http::AsyncHttpClientPtr httpClient )
{
    if (getParentId() != qnCommon->moduleGUID() || !isInitialized())
        return;
	QMutexLocker lk( &m_inputPortMutex );
    if (httpClient == m_ioHttpMonitor[0].httpClient) {
        lk.unlock();
        resetHttpClient(m_ioHttpMonitor[0].httpClient);
        startIOMonitor(Qn::PT_Input, m_ioHttpMonitor[0]);
    }
    else if (httpClient == m_ioHttpMonitor[1].httpClient) {
        lk.unlock();
        resetHttpClient(m_ioHttpMonitor[1].httpClient);
        startIOMonitor(Qn::PT_Output, m_ioHttpMonitor[1]);
    }
}

bool QnPlAxisResource::readPortSettings( CLSimpleHTTPClient* const http, QnIOPortDataList& ioPortList)
{
    QList<QPair<QByteArray,QByteArray>> params;
    CLHttpStatus status = readAxisParameters( QLatin1String("root.IOPort"), http, params );
    if( status != CL_HTTP_SUCCESS )
    {
        NX_LOG( lit("Failed to read number of input ports of camera %1. Result: %2").
            arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
        return false;
    }

    QnIOPortData portData;
    for (const auto& param: params)
    {
        QByteArray paramValue = param.second.trimmed();
        const auto nameParts = param.first.split(('.'));
        if (nameParts.size() < 3)
            continue; // bad data
        int portIndex = QString::fromLatin1(nameParts[2]).mid(1).toInt();
        QString portName = portIndexToId(portIndex);
        if (portName != portData.id) 
        {
            if(!portData.id.isEmpty())
                ioPortList.push_back(std::move(portData));
            portData = QnIOPortData();
			portData.id = portName;
        }
        QByteArray paramName;
        for (int i = 3; i < nameParts.size(); ++i) 
        {
            if (!paramName.isEmpty())
                paramName += '.';
            paramName += nameParts[i];
        }
        if (paramName == "Configurable") {
            if (paramValue == "yes")
                portData.supportedPortTypes = Qn::PT_Disabled | Qn::PT_Input | Qn::PT_Output;
            else
                portData.supportedPortTypes = Qn::PT_Disabled;
        }
        else if (paramName == "Direction") {
            if (paramValue == "input") {
                portData.supportedPortTypes |= Qn::PT_Input;
                portData.portType = Qn::PT_Input;
            }
            else if (paramValue == "output") {
                portData.supportedPortTypes |= Qn::PT_Output;
                portData.portType = Qn::PT_Output;
            }
        }
        else if (paramName == "Input.Name") {
            portData.inputName = QString::fromUtf8(paramValue);
        }
        else if (paramName == "Input.Trig") {
            if (paramValue == "closed")
                portData.iDefaultState = Qn::IO_OpenCircuit;
            else
                portData.iDefaultState = Qn::IO_GroundedCircuit;
        }
        else if (paramName == "Output.Name") {
            portData.outputName = QString::fromUtf8(paramValue);
        }
        else if (paramName == "Output.Active") {
            if (paramValue == "closed")
                portData.oDefaultState = Qn::IO_OpenCircuit;
            else
                portData.oDefaultState = Qn::IO_GroundedCircuit;
        }
        else if (paramName == "Output.PulseTime") {
            portData.autoResetTimeoutMs = paramValue.toInt();
        }
    }
    if (!params.isEmpty())
        ioPortList.push_back(std::move(portData));
    return true;
}

bool QnPlAxisResource::savePortSettings(const QnIOPortDataList& newPorts, const QnIOPortDataList& oldPorts)
{
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());

    QMap<QString,QString> changedParams;

    for(auto& currentValue: oldPorts)
    {
        QString paramNamePrefix = lit("root.IOPort.I%1.").arg(portIdToIndex(currentValue.id));
        for (const QnIOPortData& newValue: newPorts)
        {
            if (newValue.id != currentValue.id)
                continue;
            
            if (newValue.portType != Qn::PT_Disabled && newValue.portType != currentValue.portType)
                changedParams.insert(paramNamePrefix + lit("Direction"), newValue.portType == Qn::PT_Output ? lit("output") : lit("input"));

            if (newValue.inputName != currentValue.inputName)
                changedParams.insert(paramNamePrefix + lit("Input.Name"), newValue.inputName);
            if (newValue.iDefaultState != currentValue.iDefaultState)
                changedParams.insert(paramNamePrefix + lit("Input.Trig"), newValue.iDefaultState == Qn::IO_OpenCircuit ? lit("closed") : lit("open"));

            if (newValue.outputName != currentValue.outputName)
                changedParams.insert(paramNamePrefix + lit("Output.Name"), newValue.outputName);
            if (newValue.oDefaultState != currentValue.oDefaultState)
                changedParams.insert(paramNamePrefix + lit("Output.Trig"), newValue.oDefaultState == Qn::IO_OpenCircuit ? lit("closed") : lit("open"));

            if (newValue.autoResetTimeoutMs != currentValue.autoResetTimeoutMs)
                changedParams.insert(paramNamePrefix + lit("Output.PulseTime"), QString::number(newValue.autoResetTimeoutMs));
        }
    }

    QUrl updateUrl;
    updateUrl.setPath(lit("axis-cgi/param.cgi"));
    QUrlQuery urlQuery;
    static const int MAX_PATH_SIZE = 128;
    for (auto itr = changedParams.begin(); itr != changedParams.end();)
    {
        urlQuery.addQueryItem(itr.key(), itr.value());
        ++itr;
        QString path = lit("axis-cgi/param.cgi?action=update&").append(urlQuery.toString(QUrl::FullyEncoded));
        if (path.size() >= MAX_PATH_SIZE || itr == changedParams.end()) 
        {
            CLHttpStatus status = http.doGET(path);
            if (status != CL_HTTP_SUCCESS) {
                qWarning() << "Failed to update IO params for camera " << getHostAddress() << "rejected value:" << path;
                return false;
            }
            urlQuery.clear();
        }
    }
    return true;
}

QnIOPortDataList QnPlAxisResource::mergeIOSettings(const QnIOPortDataList& cameraIO, const QnIOPortDataList& savedIO)
{
    QnIOPortDataList resultIO = cameraIO;
    for (auto& result : resultIO)
        for(const auto& saved : savedIO)
            if (result.id == saved.id)
                result = saved;

    return resultIO;
}

bool QnPlAxisResource::ioPortErrorOccured()
{
    if (getCameraCapabilities() & Qn::IOModuleCapability) {
        return false; // it's error if can't read IO state for IO module
    }
    else {
        hasCameraCapabilities(getCameraCapabilities() &  ~Qn::RelayInputCapability);
        hasCameraCapabilities(getCameraCapabilities() &  ~Qn::RelayOutputCapability);
        return true;
    }
};

void QnPlAxisResource::readPortIdLIst()
{
    QString value = getProperty(Qn::IO_PORT_DISPLAY_NAMES_PARAM_NAME);
    if (value.isEmpty())
        return;
    
    QMutexLocker lock(&m_mutex);
    m_ioPortIdList.clear();
    for (const auto& portId: value.split(L','))
        m_ioPortIdList << portId;
}

bool QnPlAxisResource::initializeIOPorts( CLSimpleHTTPClient* const http )
{
    readPortIdLIst();

    if (getProperty(Qn::IO_CONFIG_PARAM_NAME).toInt() > 0)
        setCameraCapability(Qn::IOModuleCapability, true);

    QnIOPortDataList cameraPorts;
    if (!readPortSettings(http, cameraPorts))
        return ioPortErrorOccured();
    
    QnIOPortDataList savedPorts = getIOPorts();
    if (savedPorts.empty() && !cameraPorts.empty()) {
        QMutexLocker lock(&m_mutex);
        m_ioPorts = cameraPorts;
    }
    else {
        auto mergedData = mergeIOSettings(cameraPorts, savedPorts);
        if (!savePortSettings(mergedData, cameraPorts))
            return ioPortErrorOccured();
        QMutexLocker lock(&m_mutex);
        m_ioPorts = mergedData;
    }
    if (m_ioPorts != savedPorts)
        setIOPorts(m_ioPorts);

    Qn::CameraCapabilities caps = Qn::NoCapabilities;
    for (const auto& port: m_ioPorts) {
        if (port.supportedPortTypes & Qn::PT_Input)
            caps |= Qn::RelayInputCapability;
        if (port.supportedPortTypes & Qn::PT_Output)
            caps |= Qn::RelayOutputCapability;
    }
    setCameraCapabilities(getCameraCapabilities() | caps);
    return true;

    //TODO/IMPL periodically update port names in case some one changes it via camera's webpage
    //startInputPortMonitoring();
}

void QnPlAxisResource::updateIOState(const QString& portId, bool isActive, qint64 timestamp, bool overrideIfExist)
{
    QMutexLocker lock(&m_mutex);
    QnIOStateData newValue(portId, isActive, timestamp);
    bool found = false;
    for (auto& ioState: m_ioStates) {
        if (ioState.id == portId) 
        {
            if (overrideIfExist && ioState != newValue)
                ioState = newValue;
            else 
                return;
            found = true;
            break;
        }
    }
    if (!found)
        m_ioStates.push_back(newValue);
    for (const auto& port: m_ioPorts) 
    {
        if (port.id == portId) {
            if (port.portType == Qn::PT_Input) {
                lock.unlock();
                emit cameraInput(
                    toSharedPointer(),
                    portId,
                    isActive,
                    timestamp );
            }
            else if (port.portType == Qn::PT_Output) {
                lock.unlock();
                emit cameraOutput(
                    toSharedPointer(),
                    portId,
                    isActive,
                    timestamp );
            }
            break;
        }
    }
}

QnIOStateDataList QnPlAxisResource::ioStates() const
{
    QMutexLocker lock(&m_mutex);
    return m_ioStates;
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


    QString portDisplayName = QString::fromLatin1(notification.mid(0, sepPos));
    int portIndex = portDisplayNameToIndex(portDisplayName);
    QString portId = portIndexToId(portIndex);
    const char portType = notification[portTypePos];
    NX_LOG( lit("%1 port %2 changed its state to %3. Camera %4").
        arg(QLatin1String(portType == 'I' ? "Input" : "Output")).arg(portId).arg(QLatin1String(eventType == '/' || eventType == 'H' ? "active" : "inactive")).arg(getUrl()), cl_logDEBUG1 );

    if (eventType != '/' && eventType != '\\' /*&& eventType != 'H' && eventType != 'L'*/)
        return; // skip unknown event

    bool isOnState = (eventType == '/') || (eventType == 'H');
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    updateIOState(portId, isOnState, timestamp, true);
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

bool QnPlAxisResource::readCurrentIOStateAsync()
{
    if( hasFlags(Qn::foreigner) ||      //we do not own camera
        m_ioPorts.empty() )    //camera report no io 
    {
        return false;
    }

    //based on VAPIX Version 3 I/O Port API

    const QAuthenticator& auth = getAuth();
    QUrl requestUrl;
    requestUrl.setHost( getHostAddress() );
    requestUrl.setPort( QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT) );
    //preparing request
    QString requestPath = lit("/axis-cgi/io/port.cgi?checkactive=");

    QString portList;
    for (const auto& port: getIOPorts()) {
        if (port.portType != Qn::PT_Disabled) {
            if (!portList.isEmpty())
                portList += lit(",");
            portList += portIndexToReqParam(portIdToIndex(port.id));
        }
    }
    if (portList.isEmpty())
        return false; // no ports to listen

    requestPath += portList;
    requestUrl.setPath( requestPath );
    
    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &QnPlAxisResource::onCurrentIOStateResponseReceived,
        Qt::DirectConnection );
    httpClient->setTotalReconnectTries( nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
    httpClient->setUserName( auth.user() );
    httpClient->setUserPassword( auth.password() );

    if( !httpClient->doGet( requestUrl ) )
        return false;

    m_inputPortStateReader = std::move( httpClient );
    return true;
}

void QnPlAxisResource::asyncUpdateIOSettings(const QString & key)
{
    QN_UNUSED(key);

    const auto newValue = QJson::deserialized<QnIOPortDataList>(getProperty(Qn::IO_SETTINGS_PARAM_NAME).toUtf8());
    QnIOPortDataList prevValue;
    {
        QMutexLocker lock(&m_mutex);
        if (newValue == m_ioPorts)
            return;
        prevValue = m_ioPorts;
    }

    stopInputPortMonitoringAsync();
    if (savePortSettings(newValue, prevValue)) 
    {
        {
            QMutexLocker lock(&m_mutex);
            m_ioPorts = newValue;
        }
        startInputPortMonitoringAsync( std::function<void(bool)>() );
    }
    else
        setStatus(Qn::Offline); // reinit
}

void QnPlAxisResource::at_propertyChanged(const QnResourcePtr & /*res*/, const QString & key)
{
    if (key == Qn::IO_SETTINGS_PARAM_NAME)
        QnConcurrent::run(QThreadPool::globalInstance(), std::bind(&QnPlAxisResource::asyncUpdateIOSettings, this, key));
}

#endif // #ifdef ENABLE_AXIS
