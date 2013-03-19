
#include "axis_resource.h"

#include <functional>
#include <memory>

#include "api/app_server_connection.h"
#include "../onvif/dataprovider/onvif_mjpeg.h"
#include "axis_stream_reader.h"
#include <business/business_event_connector.h>
#include <business/business_event_rule.h>
#include <business/business_rule_processor.h>
#include "utils/common/synctime.h"
#include "axis_ptz_controller.h"

using namespace std;

const char* QnPlAxisResource::MANUFACTURE = "Axis";
static const float MAX_AR_EPS = 0.04f;
static const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
static const quint16 DEFAULT_AXIS_API_PORT = 80;

QnPlAxisResource::QnPlAxisResource()
{
    setAuth(QLatin1String("root"), QLatin1String("root"));
    m_lastMotionReadTime = 0;
    connect(
        this, SIGNAL(cameraInput(QnResourcePtr, const QString&, bool, qint64)), 
        QnBusinessEventConnector::instance(), SLOT(at_cameraInput(QnResourcePtr, const QString&, bool, qint64)) );
}

QnPlAxisResource::~QnPlAxisResource()
{
    stopInputPortMonitoring();
}

bool QnPlAxisResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlAxisResource::updateMACAddress()
{
    return true;
}

QString QnPlAxisResource::manufacture() const
{
    return QLatin1String(MANUFACTURE);
}

void QnPlAxisResource::setIframeDistance(int /*frames*/, int /*timems*/)
{

}

QnAbstractStreamDataProvider* QnPlAxisResource::createLiveDataProvider()
{
    return new QnAxisStreamReader(toSharedPointer());
}

bool QnPlAxisResource::shoudResolveConflicts() const 
{
    return false;
}

void QnPlAxisResource::setCropingPhysical(QRect /*croping*/)
{

}

bool QnPlAxisResource::startInputPortMonitoring()
{
    if( isDisabled()
        || hasFlags(QnResource::foreigner)      //we do not own camera
        || m_inputPortNameToIndex.empty() )
    {
        return false;
    }

    //based on VAPIX Version 3 I/O Port API

    const QAuthenticator& auth = getAuth();
    QUrl requestUrl;
    requestUrl.setHost( getHostAddress() );
    requestUrl.setPort( QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT) );
    for( std::map<QString, unsigned int>::const_iterator
        it = m_inputPortNameToIndex.begin();
        it != m_inputPortNameToIndex.end();
        ++it )
    {
        QMutexLocker lk( &m_inputPortMutex );
        pair<map<unsigned int, nx_http::AsyncHttpClient*>::iterator, bool>
            p = m_inputPortHttpMonitor.insert( make_pair( it->second, (nx_http::AsyncHttpClient*)NULL ) );
        if( !p.second )
            continue;   //port already monitored
        lk.unlock();

        //it is safe to proceed with no lock futher because stopInputMonitoring can be only called from current thread 
            //and forgetHttpClient cannot be called before doGet call

        requestUrl.setPath( QString::fromLatin1("/axis-cgi/io/port.cgi?monitor=%1").arg(it->second) );
        nx_http::AsyncHttpClient* httpClient = new nx_http::AsyncHttpClient();
        connect( httpClient, SIGNAL(responseReceived(nx_http::AsyncHttpClient*)),          this, SLOT(onMonitorResponseReceived(nx_http::AsyncHttpClient*)),        Qt::DirectConnection );
        connect( httpClient, SIGNAL(someMessageBodyAvailable(nx_http::AsyncHttpClient*)),  this, SLOT(onMonitorMessageBodyAvailable(nx_http::AsyncHttpClient*)),    Qt::DirectConnection );
        connect( httpClient, SIGNAL(done(nx_http::AsyncHttpClient*)),                      this, SLOT(onMonitorConnectionClosed(nx_http::AsyncHttpClient*)),        Qt::DirectConnection );
        httpClient->setTotalReconnectTries( nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
        httpClient->setUserName( auth.user() );
        httpClient->setUserPassword( auth.password() );
        httpClient->doGet( requestUrl );

        p.first->second = httpClient;
    }

    return true;
}

void QnPlAxisResource::stopInputPortMonitoring()
{
    QMutexLocker lk( &m_inputPortMutex );

    nx_http::AsyncHttpClient* httpClient = NULL;
    while( !m_inputPortHttpMonitor.empty() )
    {
        httpClient = m_inputPortHttpMonitor.begin()->second;
        m_inputPortHttpMonitor.erase( m_inputPortHttpMonitor.begin() );
        lk.unlock();
        httpClient->terminate();
        httpClient->scheduleForRemoval();
        lk.relock();
    }
}

bool QnPlAxisResource::isInputPortMonitored() const
{
    QMutexLocker lk( &m_inputPortMutex );
    return !m_inputPortHttpMonitor.empty();
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
            setStatus(QnResource::Unauthorized);
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

bool QnPlAxisResource::initInternal()
{

    {
        // enable send motion into H.264 stream
        CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
        //CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.I0.MPEG.UserDataEnabled=yes")); 
        CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.TriggerDataEnabled=yes&Audio.A0.Enabled=").append(isAudioEnabled() ? "yes" : "no"));
        //CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.I0.MPEG.UserDataEnabled=yes&Image.I1.MPEG.UserDataEnabled=yes&Image.I2.MPEG.UserDataEnabled=yes&Image.I3.MPEG.UserDataEnabled=yes"));
        if (status != CL_HTTP_SUCCESS) {
            if (status == CL_HTTP_AUTH_REQUIRED)
                setStatus(QnResource::Unauthorized);
            return false;
        }
    }

    readMotionInfo();

    // determin camera max resolution
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());
    CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=list&group=Properties.Image.Resolution"));
    if (status != CL_HTTP_SUCCESS) {
        if (status == CL_HTTP_AUTH_REQUIRED)
            setStatus(QnResource::Unauthorized);
        return false;
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
            return false;
        }

        m_palntscRes = false;

        m_resolutionList = body.mid(paramValuePos+1).split(',');
        for (int i = 0; i < m_resolutionList.size(); ++i)
        {
            m_resolutionList[i] = m_resolutionList[i].toLower().trimmed();

            
            if (m_resolutionList[i]=="qcif")
            {
                m_resolutionList[i] = "176x144";
                m_palntscRes = true;
            }

            else if (m_resolutionList[i]=="cif")
            {
                m_resolutionList[i] = "352x288";
                m_palntscRes = true;
            }

            else if (m_resolutionList[i]=="2cif")
            {
                m_resolutionList[i] = "704x288";
                m_palntscRes = true;
            }

            else if (m_resolutionList[i]=="4cif")
            {
                m_resolutionList[i] = "704x576";
                m_palntscRes = true;
            }

            else if (m_resolutionList[i]=="d1")
            {
                m_resolutionList[i] = "720x576";
                m_palntscRes = true;
            }
        }
    }   //releasing mutex so that not to make other threads using the resource to wait for completion of heavy-wait io & pts initialization,
            //m_initMutex is locked up the stack
    


    //root.Image.MotionDetection=no
    //root.Image.I0.TriggerData.MotionDetectionEnabled=yes
    //root.Image.I1.TriggerData.MotionDetectionEnabled=yes
    //root.Properties.Motion.MaxNbrOfWindows=10

    //TODO/IMPL check firmware version. it must be >= 5.0.0 to support I/O ports

    initializeIOPorts( &http );

    initializePtz(&http);

    // TODO: #Elric this is totally evil, copypasta from ONVIF resource.
    {
        QnAppServerConnectionPtr conn = QnAppServerConnectionFactory::createConnection();
        if (conn->saveSync(toSharedPointer().dynamicCast<QnVirtualCameraResource>()) != 0)
            qnCritical("Can't save resource %1 to Enterprise Controller. Error: %2.", getName(), conn->getLastError());
    }

    return true;
}

QByteArray QnPlAxisResource::getMaxResolution() const
{
    QMutexLocker lock(&m_mutex);

    if (m_palntscRes)
        return QByteArray("D1");

    return !m_resolutionList.isEmpty() ? m_resolutionList[0] : QByteArray();
}

float QnPlAxisResource::getResolutionAspectRatio(const QByteArray& resolution) const
{
    QList<QByteArray> dimensions = resolution.split('x');
    if (dimensions.size() != 2)
    {
        qWarning() << Q_FUNC_INFO << "invalid resolution format. Expected widthxheight";
        return 1.0;
    }
    return dimensions[0].toFloat()/dimensions[1].toFloat();
}


QString QnPlAxisResource::getNearestResolution(const QByteArray& resolution, float aspectRatio) const
{
    QMutexLocker lock(&m_mutex);

    if (m_palntscRes)
        return QLatin1String("CIF");



    QList<QByteArray> dimensions = resolution.split('x');
    if (dimensions.size() != 2)
    {
        qWarning() << Q_FUNC_INFO << "invalid request resolution format. Expected widthxheight";
        return QString();
    }
    float requestSquare = dimensions[0].toInt() * dimensions[1].toInt();
    int bestIndex = -1;
    float bestMatchCoeff = (float)INT_MAX;
    for (int i = 0; i < m_resolutionList.size(); ++ i)
    {
        float ar = getResolutionAspectRatio(m_resolutionList[i]);
        if (qAbs(ar-aspectRatio) > MAX_AR_EPS)
            continue;
        dimensions = m_resolutionList[i].split('x');
        if (dimensions.size() != 2)
            continue;
        float square = dimensions[0].toInt() * dimensions[1].toInt();
        float matchCoeff = qMax(requestSquare, square) / qMin(requestSquare, square);
        if (matchCoeff < bestMatchCoeff)
        {
            bestIndex = i;
            bestMatchCoeff = matchCoeff;
        }
    }
    return bestIndex >= 0 ? QLatin1String(m_resolutionList[bestIndex]) : QLatin1String(resolution);
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

    const QnMotionRegion region = m_motionMaskList[0];

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

const QnResourceAudioLayout* QnPlAxisResource::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider)
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


int QnPlAxisResource::getChannelNum() const
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
	{	// functor for unary first of pair selector operator
	const typename _Pair::first_type& operator()(const _Pair& _Left) const
		{	// apply first selector operator to pair operand
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

    QString cmd = QString::fromLatin1("axis-cgi/io/port.cgi?action=%1:%2").arg(it->second+1).arg(QLatin1String(activate ? "/" : "\\"));
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
        cl_log.log( QString::fromLatin1("Failed to set camera %1 port %2 output state to %3. Result: %4").
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
    CLHttpStatus status = httpClient->doGET( QString::fromLatin1("axis-cgi/param.cgi?action=list&group=%1").arg(paramName).toLatin1() );
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
            cl_log.log( QString::fromLatin1("Failed to read param %1 of camera %2. Unexpected response: %3").
                arg(paramName).arg(getHostAddress()).arg(QLatin1String(body)), cl_logWARNING );
            return CL_HTTP_BAD_REQUEST;
        }
    }
    else
    {
        cl_log.log( QString::fromLatin1("Failed to param %1 of camera %2. Result: %3").
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

void QnPlAxisResource::onMonitorResponseReceived( nx_http::AsyncHttpClient* const httpClient )
{
    Q_ASSERT( httpClient );

    if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        cl_log.log( QString::fromLatin1("Axis camera %1. Failed to subscribe to input %2 monitoring. %3").
            arg(getUrl()).arg(QLatin1String("")).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );
        forgetHttpClient( httpClient );
        return;
    }

    static const char* multipartContentType = "multipart/x-mixed-replace";

    //analyzing response headers (if needed)
    const nx_http::StringType& contentType = httpClient->contentType();
    const nx_http::StringType::value_type* sepPos = std::find( contentType.constData(), contentType.constData()+contentType.size(), ';' );
    if( sepPos == contentType.constData()+contentType.size() ||
        nx_http::ConstBufferRefType(contentType, 0, sepPos-contentType.constData()) != multipartContentType )
    {
        //unexpected content type
        cl_log.log( QString::fromLatin1("Error monitoring axis camera %1. Unexpected Content-Type (%2) in monitor response, Expected: %3").
            arg(getUrl()).arg(QLatin1String(contentType)).arg(QLatin1String(multipartContentType)), cl_logWARNING );
        //deleting httpClient
        forgetHttpClient( httpClient );
        return;
    }

    const nx_http::StringType::value_type* boundaryStart = std::find_if(
        sepPos+1,
        contentType.constData()+contentType.size(),
        std::not1( std::bind1st( std::equal_to<nx_http::StringType::value_type>(), ' ' ) ) );   //searching first non-space
    if( boundaryStart == contentType.constData()+contentType.size() )
    {
        //failed to read boundary marker
        cl_log.log( QString::fromLatin1("Error monitoring axis camera %1. Missing boundary marker in content-type %2 (1)").
            arg(getUrl()).arg(QLatin1String(contentType)), cl_logWARNING );
        //deleting httpClient
        forgetHttpClient( httpClient );
        return;
    }
    if( !nx_http::ConstBufferRefType(contentType, boundaryStart-contentType.constData()).startsWith("boundary=") )
    {
        //failed to read boundary marker
        cl_log.log( QString::fromLatin1("Error monitoring axis camera %1. Missing boundary marker in content-type %2 (2)").
            arg(getUrl()).arg(QLatin1String(contentType)), cl_logWARNING );
        //deleting httpClient
        forgetHttpClient( httpClient );
        return;
    }
    boundaryStart += sizeof("boundary=")-1;
    m_multipartContentParser.setBoundary( contentType.mid( boundaryStart-contentType.constData() ) );

    httpClient->startReadMessageBody();
}

void QnPlAxisResource::onMonitorMessageBodyAvailable( nx_http::AsyncHttpClient* const httpClient )
{
    Q_ASSERT( httpClient );

    const nx_http::BufferType& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
    for( int offset = 0; offset < msgBodyBuf.size(); )
    {
        size_t bytesProcessed = 0;
        nx_http::MultipartContentParser::ResultCode resultCode = m_multipartContentParser.parseBytes(
            nx_http::ConstBufferRefType(msgBodyBuf, offset),
            &bytesProcessed );
        offset += (int) bytesProcessed;
        switch( resultCode )
        {
            case nx_http::MultipartContentParser::partDataDone:
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

            case nx_http::MultipartContentParser::someDataAvailable:
                if( !m_multipartContentParser.prevFoundData().isEmpty() )
                    m_currentMonitorData.append(
                        m_multipartContentParser.prevFoundData().data(),
                        (int) m_multipartContentParser.prevFoundData().size() );
                break;

            case nx_http::MultipartContentParser::eof:
                //TODO/IMPL reconnect
                break;

            default:
                continue;
        }
    }
}

void QnPlAxisResource::onMonitorConnectionClosed( nx_http::AsyncHttpClient* const /*httpClient*/ )
{
    //TODO/IMPL reconnect
}

void QnPlAxisResource::initializeIOPorts( CLSimpleHTTPClient* const http )
{
    unsigned int inputPortCount = 0;
    CLHttpStatus status = readAxisParameter( http, QLatin1String("Input.NbrOfInputs"), &inputPortCount );
    if( status != CL_HTTP_SUCCESS )
        cl_log.log( QString::fromLatin1("Failed to read number of input ports of camera %1. Result: %2").
            arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
    else if( inputPortCount > 0 )
        setCameraCapability(Qn::RelayInputCapability, true);

    unsigned int outputPortCount = 0;
    status = readAxisParameter( http, QLatin1String("Output.NbrOfOutputs"), &outputPortCount );
    if( status != CL_HTTP_SUCCESS )
        cl_log.log( QString::fromLatin1("Failed to read number of output ports of camera %1. Result: %2").
            arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
    else if( outputPortCount > 0 )
        setCameraCapability(Qn::RelayOutputCapability, true);

    //reading port direction and names
    for( unsigned int i = 0; i < inputPortCount+outputPortCount; ++i )
    {
        QString portDirection;
        status = readAxisParameter( http, QString::fromLatin1("IOPort.I%1.Direction").arg(i), &portDirection );
        if( status != CL_HTTP_SUCCESS )
        {
            cl_log.log( QString::fromLatin1("Failed to read name of port %1 of camera %2. Result: %3").
                arg(i).arg(getHostAddress()).arg(::toString(status)), cl_logWARNING );
            continue;
        }

        QString portName;
        status = readAxisParameter( http, QString::fromLatin1("IOPort.I%1.%2.Name").arg(i).arg(portDirection), &portName );
        if( status != CL_HTTP_SUCCESS )
        {
            cl_log.log( QString::fromLatin1("Failed to read name of input port %1 of camera %2. Result: %3").
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
    cl_log.log( QString::fromLatin1("Received notification %1 from %2").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logDEBUG1 );

    //notification
    size_t sepPos = nx_http::find_first_of( notification, ":" );
    if( sepPos == nx_http::BufferNpos || sepPos+1 >= notification.size() )
    {
        cl_log.log( QString::fromLatin1("Error parsing notification %1 from %2. Event type not found").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logINFO );
        return;
    }
    const char eventType = notification[sepPos+1];
    size_t portTypePos = nx_http::find_first_not_of( notification, "0123456789" );
    if( portTypePos == nx_http::BufferNpos )
    {
        cl_log.log( QString::fromLatin1("Error parsing notification %1 from %2. Port type not found").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logINFO );
        return;
    }
    const unsigned int portNumber = notification.mid( 0, portTypePos ).toUInt();
    const char portType = notification[portTypePos];
    cl_log.log( QString::fromLatin1("%1 port %2 changed its state to %3. Camera %4").
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

void QnPlAxisResource::forgetHttpClient( nx_http::AsyncHttpClient* const httpClient )
{
    QMutexLocker lk( &m_inputPortMutex );

    for( std::map<unsigned int, nx_http::AsyncHttpClient*>::iterator
        it = m_inputPortHttpMonitor.begin();
        it != m_inputPortHttpMonitor.end();
        ++it )
    {
        if( it->second == httpClient )
        {
            it->second->scheduleForRemoval();
            m_inputPortHttpMonitor.erase( it );
            return;
        }
    }
}

void QnPlAxisResource::initializePtz(CLSimpleHTTPClient *http) {
    Q_UNUSED(http)
    // TODO: make configurable.
    static const char *brokenPtzCameras[] = {"AXISP3344", "AXISP1344", NULL};

    // TODO: use QHash here, +^
    QString localModel = getModel();
    for(const char **model = brokenPtzCameras; *model; model++)
        if(localModel == QLatin1String(*model))
            return;

    m_ptzController.reset(new QnAxisPtzController(this));
    Qn::CameraCapabilities capabilities = m_ptzController->getCapabilities();
    if(capabilities == Qn::NoCapabilities)
        m_ptzController.reset();

    setCameraCapabilities((getCameraCapabilities() & ~Qn::AllPtzCapabilities) | capabilities);
}

QnAbstractPtzController* QnPlAxisResource::getPtzController() {
    return m_ptzController.data();
}
