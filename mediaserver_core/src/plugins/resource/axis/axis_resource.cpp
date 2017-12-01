#ifdef ENABLE_AXIS

#include "axis_resource.h"

#include <algorithm>
#include <functional>
#include <memory>

#include <api/app_server_connection.h>

#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>

#include <plugins/resource/onvif/dataprovider/onvif_mjpeg.h>
#include <core/resource_management/resource_pool.h>

#include "axis_stream_reader.h"
#include "axis_ptz_controller.h"
#include "axis_audio_transmitter.h"

#include <api/model/api_ioport_data.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/concurrent.h>
#include <utils/xml/camera_advanced_param_reader.h>
#include <common/common_module.h>
#include <core/resource_management/resource_data_pool.h>

#include <motion/motion_detection.h>
#include <common/static_common_module.h>

#include <algorithm>

using namespace std;

const QString QnPlAxisResource::MANUFACTURE(lit("Axis"));

namespace {
    const static std::chrono::milliseconds kReconnectTimeout(1000);
    const float MAX_AR_EPS = 0.04f;
    const quint64 MOTION_INFO_UPDATE_INTERVAL = 1000000ll * 60;
    const quint16 DEFAULT_AXIS_API_PORT = 80;
    const int AXIS_IO_KEEP_ALIVE_TIME = 1000 * 15;
    const QString AXIS_SUPPORTED_AUDIO_CODECS_PARAM_NAME("Properties.Audio.Decoder.Format");
    const QString AXIS_TWO_WAY_AUDIO_MODES("Properties.Audio.DuplexMode");
    const QString AXIS_FIRMWARE_VERSION_PARAM_NAME("Properties.Firmware.Version");

    QnAudioFormat toAudioFormat(const QString& codecName)
    {
        QnAudioFormat result;
        if (codecName == lit("g711"))
        {
            result.setSampleRate(8000);
            result.setCodec("MULAW");
        }
        else if (codecName == lit("g726"))
        {
            result.setSampleRate(8000);
            result.setCodec("G726");
        }
        else if (codecName == lit("axis-mulaw-128"))
        {
            result.setSampleRate(16000);
            result.setCodec("MULAW");
        }

        return result;
    }
}

int QnPlAxisResource::portIdToIndex(const QString& id) const
{
    QnMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_ioPortIdList.size(); ++i) {
        if (m_ioPortIdList[i] == id)
            return i;
    }
    return id.mid(1).toInt();
}

QString QnPlAxisResource::portIndexToId(int num) const
{
    QnMutexLocker lock(&m_mutex);
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

class AxisIOMessageBodyParser: public nx::utils::bstream::AbstractByteStreamFilter
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

QnPlAxisResource::QnPlAxisResource():
    m_lastMotionReadTime(0),
    m_inputIoMonitor(Qn::PT_Input),
    m_outputIoMonitor(Qn::PT_Output)
{
    m_audioTransmitter.reset(new QnAxisAudioTransmitter(this));
    setVendor(lit("Axis"));
    connect( this, &QnResource::propertyChanged, this, &QnPlAxisResource::at_propertyChanged, Qt::DirectConnection );
}

QnPlAxisResource::~QnPlAxisResource()
{
    m_audioTransmitter.reset();
    stopInputPortMonitoringSync();
    m_timer.pleaseStopSync(/*checkForLocks*/ false);
}

void QnPlAxisResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    QUrl apiUrl;
    apiUrl.setScheme( lit("http") );
    apiUrl.setHost( getHostAddress() );
    apiUrl.setPort( QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT) );

    QAuthenticator auth = getAuth();

    apiUrl.setUserName( auth.user() );
    apiUrl.setPassword( auth.password() );
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

    nx_http::downloadFileAsync(
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
        QnMutexLocker lock(&m_mutex);
        m_ioStates.clear();
    }
    m_timer.post([this]() { startInputPortMonitoring(); });
    return true;
}

void QnPlAxisResource::startInputPortMonitoring()
{
    startIOMonitorInternal(m_inputIoMonitor);
    if (isIOModule())
        startIOMonitorInternal(m_outputIoMonitor);
    readCurrentIOStateAsync();
}

bool QnPlAxisResource::startIOMonitorInternal(IOMonitor& ioMonitor)
{
    if( hasFlags(Qn::foreigner) ||      //we do not own camera
        m_ioPorts.empty() )    //camera report no io
    {
        return false;
    }
    if (ioMonitor.httpClient)
        return true; //< already started

    //based on VAPIX Version 3 I/O Port API

    QAuthenticator auth = getAuth();
    QUrl requestUrl;
    requestUrl.setHost( getHostAddress() );
    requestUrl.setPort( QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT) );
    //preparing request
    requestUrl.setPath(lit("/axis-cgi/io/port.cgi"));
    QString requestQuery = lit("monitor=");

    QString portList;
    for (const auto& port: getIOPorts())
    {
        if (port.portType == ioMonitor.portType)
        {
            if (!portList.isEmpty())
                portList += lit(",");
            portList += portIndexToReqParam(portIdToIndex(port.id));
        }
    }
    if (portList.isEmpty())
        return false; // no ports to listen

    requestQuery += portList;
    requestUrl.setQuery(requestQuery);

    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    httpClient->bindToAioThread(m_timer.getAioThread());
    connect( httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnPlAxisResource::onMonitorResponseReceived, Qt::DirectConnection );
    connect( httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable, this, &QnPlAxisResource::onMonitorMessageBodyAvailable, Qt::DirectConnection );
    connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnPlAxisResource::onMonitorConnectionClosed, Qt::DirectConnection );
    httpClient->setTotalReconnectTries( nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
    httpClient->setUserName( auth.user() );
    httpClient->setUserPassword( auth.password() );
    httpClient->setMessageBodyReadTimeoutMs(AXIS_IO_KEEP_ALIVE_TIME * 2);

    ioMonitor.contentParser = std::make_shared<nx_http::MultipartContentParser>();
    ioMonitor.contentParser->setNextFilter(std::make_shared<AxisIOMessageBodyParser>(this));

    httpClient->doGet(requestUrl);

    ioMonitor.httpClient = httpClient;
    return true;
}

void QnPlAxisResource::stopInputPortMonitoringAsync()
{
    m_timer.post(
        [this]()
        {
            stopInputPortMonitoringSync();
        }
    );
}

void QnPlAxisResource::stopInputPortMonitoringSync()
{
    m_inputIoMonitor.httpClient.reset();
    m_outputIoMonitor.httpClient.reset();
    m_inputPortStateReader.reset();
    m_timer.cancelSync();

    auto timeMs = qnSyncTime->currentMSecsSinceEpoch();
    for (const auto& state : ioStates())
    {
        if (state.isActive)
            updateIOState(state.id, false /*inactive*/, timeMs, true /*override*/);
    }
}

bool QnPlAxisResource::isInputPortMonitored() const
{
    return m_inputIoMonitor.httpClient != nullptr;
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
    qreal xCoeff = 9999.0 / (Qn::kMotionGridWidth-1);
    qreal yCoeff = 9999.0 / (Qn::kMotionGridHeight-1);
    QPoint topLeft(Qn::kMotionGridWidth - (axisRect.left()/xCoeff + 0.5), Qn::kMotionGridHeight - (axisRect.top()/yCoeff + 0.5));
    QPoint bottomRight(Qn::kMotionGridWidth - (axisRect.right()/xCoeff + 0.5), Qn::kMotionGridHeight - (axisRect.bottom()/yCoeff + 0.5));
    return QRect(topLeft, bottomRight).normalized();
}

QRect QnPlAxisResource::gridRectToAxisRect(const QRect& gridRect)
{
    qreal xCoeff = 9999.0 / Qn::kMotionGridWidth;
    qreal yCoeff = 9999.0 / Qn::kMotionGridHeight;
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

    updateDefaultAuthIfEmpty(QLatin1String("root"), QLatin1String("root"));
    QAuthenticator auth = getAuth();

    //TODO #ak check firmware version. it must be >= 5.0.0 to support I/O ports
    {
        CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), auth);

        QString firmware;
        auto status = readAxisParameter(&http, AXIS_FIRMWARE_VERSION_PARAM_NAME, &firmware);
        if (status == CL_HTTP_SUCCESS)
            setFirmware(firmware);
    }

    if (hasVideo(0))
    {
        // enable send motion into H.264 stream
        CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), auth);
        //CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.I0.MPEG.UserDataEnabled=yes"));
        CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.TriggerDataEnabled=yes&Audio.A0.Enabled=yes"));
        //CLHttpStatus status = http.doGET(QByteArray("axis-cgi/param.cgi?action=update&Image.I0.MPEG.UserDataEnabled=yes&Image.I1.MPEG.UserDataEnabled=yes&Image.I2.MPEG.UserDataEnabled=yes&Image.I3.MPEG.UserDataEnabled=yes"));
        if (status != CL_HTTP_SUCCESS) {
            if (status == CL_HTTP_AUTH_REQUIRED)
                setStatus(Qn::Unauthorized);
            return CameraDiagnostics::UnknownErrorResult();
        }
    }

    {
        //reading RTSP port
        CLSimpleHTTPClient http( getHostAddress(), QUrl( getUrl() ).port( DEFAULT_AXIS_API_PORT ), getNetworkTimeout(), auth );
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

    // Determine camera max resolution.
    CLSimpleHTTPClient http (getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), auth);
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

        if (!isIOModule() && m_resolutionList.isEmpty())
            return CameraDiagnostics::CameraInvalidParams("Failed to read resolution list");
    }

    if (hasVideo(0))
    {
        QnMutexLocker lock(&m_mutex);
        std::sort(m_resolutionList.begin(), m_resolutionList.end(), resolutionGreatThan);

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


    if(!initializeAudio(&http))
        return CameraDiagnostics::UnknownErrorResult();

    enableDuplexMode();

    /* Ptz capabilities will be initialized by PTZ controller pool. */

    fetchAndSetAdvancedParameters();

    // Copy information from build-in resource type to runtime params
    // because mobile client doesn't load resource type information.
    if (isIOModule())
        setProperty(Qn::IO_CONFIG_PARAM_NAME, QString("1"));

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
        QnMutexLocker lock(&m_mutex);
        for (const auto& value: m_ioPorts) {
            if (value.portType == Qn::PT_Output)
                portNum = portIdToIndex(value.id);
        }
    }
    else {
        portNum = portIdToIndex(outputID);
    }
    QString cmd = lit("axis-cgi/io/port.cgi?action=%1:%2").arg(portIndexToReqParam(portNum)).arg(QLatin1String(activate ? "/" : "\\"));
    if (autoResetTimeoutMS > 0)
    {
        //adding auto-reset
        cmd += QString::number(autoResetTimeoutMS)+QLatin1String(activate ? "\\" : "");
    }

    CLSimpleHTTPClient httpClient( getHostAddress(), QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT), getNetworkTimeout(), getAuth());

    //cmd = QLatin1String("/axis-cgi/param.cgi?action=list&group=IOPort.I0.Configurable");
    //cmd = QLatin1String("/axis-cgi/param.cgi?action=list&group=IOPort.I1.Output.Name");
    //cmd = QLatin1String("/axis-cgi/io/port.cgi?check=1,2");
    //cmd = QLatin1String("/axis-cgi/io/port.cgi?checkactive=1,2");
    //cmd = QLatin1String("/axis-cgi/io/port.cgi?action=2:/300\\500/300\\");
    //cmd = QLatin1String("/axis-cgi/param.cgi?action=list&group=IOPort.I1");

    CLHttpStatus status = httpClient.doGET( cmd );
    if (status / 100 != 2)
    {
        NX_LOG( lit("Failed to set camera %1 port %2 output state to %3. Result: %4").
            arg(getHostAddress()).arg(outputID).arg(activate).arg(::toString(status)), cl_logWARNING );
        return false;
    }

    return true;
}

bool QnPlAxisResource::enableDuplexMode() const
{
    CLSimpleHTTPClient http (
        getHostAddress(),
        QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT),
        getNetworkTimeout(),
        getAuth());

    auto status = http.doGET(lit("/axis-cgi/param.cgi?action=update&root.Audio.DuplexMode=full"));

    return status == CL_HTTP_SUCCESS;
}

CLHttpStatus QnPlAxisResource::readAxisParameters(
    const QString& rootPath,
    CLSimpleHTTPClient* const httpClient,
    QList<QPair<QByteArray,QByteArray>>& params)
{
    params.clear();
    CLHttpStatus status = httpClient->doGET( lit("axis-cgi/param.cgi?action=list&group=%1").arg(rootPath).toLatin1() );
    if (status == CL_HTTP_SUCCESS)
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

CLHttpStatus QnPlAxisResource::readAxisParameters(
    const QString& rootPath,
    CLSimpleHTTPClient* const httpClient,
    QMap<QString, QString>& params)
{
    params.clear();
    CLHttpStatus status = httpClient->doGET( lit("axis-cgi/param.cgi?action=list&group=%1").arg(rootPath).toLatin1() );
    if (status == CL_HTTP_SUCCESS)
    {
        QByteArray body;
        httpClient->readAll( body );
        for (const QByteArray& line: body.split('\n'))
        {
            const auto& paramItems = line.split('=');
            if( paramItems.size() == 2)
                params[paramItems[0]] = paramItems[1];
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
    QList<QPair<QByteArray,QByteArray>> params;
    auto result = readAxisParameters(paramName, httpClient, params);
    if (result != CL_HTTP_SUCCESS)
        return result;
    for (auto itr = params.constBegin(); itr != params.constEnd(); ++itr)
    {
        if (itr->first == paramName)
        {
            *paramValue = itr->second;
            return CL_HTTP_SUCCESS;
        }
    }
    return CL_HTTP_BAD_REQUEST;
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

bool QnPlAxisResource::initializeAudio(CLSimpleHTTPClient * const http)
{
    QString duplexModeList;
    auto status = readAxisParameter(http, AXIS_TWO_WAY_AUDIO_MODES, &duplexModeList);
    if (status != CLHttpStatus::CL_HTTP_SUCCESS)
    {
        setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, QString("0"));
        return true;
    }
    const QSet<QString> supportedModes = duplexModeList.split(',').toSet();
    const QSet<QString> kToCameraModes = {"full","half","get","speaker"};
    const QSet<QString> kFromCameraModes = {"full","half","post"};

    const bool twoWayAudioFound = supportedModes.intersects(kToCameraModes);
    const bool fromCameraAudioFound = supportedModes.intersects(kFromCameraModes);

    if (fromCameraAudioFound)
        setProperty(Qn::IS_AUDIO_SUPPORTED_PARAM_NAME, QString("1"));

    if (!twoWayAudioFound)
        return true;

    QString outputFormats;
    status = readAxisParameter(http, AXIS_SUPPORTED_AUDIO_CODECS_PARAM_NAME, &outputFormats);
    if (status != CLHttpStatus::CL_HTTP_SUCCESS)
        return true;

    for (const auto& formatStr: outputFormats.split(','))
    {
        QnAudioFormat outputFormat = toAudioFormat(formatStr);
        if (m_audioTransmitter->isCompatible(outputFormat))
        {
            m_audioTransmitter->setOutputFormat(outputFormat);
            setCameraCapabilities(getCameraCapabilities() | Qn::AudioTransmitCapability);
        }
    }

    return true;
}

void QnPlAxisResource::onMonitorResponseReceived( nx_http::AsyncHttpClientPtr httpClient )
{
    NX_ASSERT( httpClient );

    IOMonitor* ioMonitor = ioMonitorByHttpClient(httpClient);
    if (!ioMonitor)
        return;

    if (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        NX_LOG( lit("Axis camera %1. Failed to subscribe to input port(s) monitoring. %2").
            arg(getUrl()).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );
        return;
    }

    //analyzing response headers (if needed)
    if (!ioMonitor->contentParser->setContentType(httpClient->contentType()))
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
    NX_ASSERT( httpClient );

    if (httpClient->failed())
    {
        NX_LOG( lit("Axis camera %1. Failed to read current IO state. No HTTP response").arg(getUrl()), cl_logWARNING );
    }
    else if (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        NX_LOG( lit("Axis camera %1. Failed to read current IO state. %2").
            arg(getUrl()).arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logWARNING );
    }
    else
    {
        QList<QByteArray> result = httpClient->fetchMessageBodyBuffer().split('\n');
        for (const auto& line: result)
        {
            QList<QByteArray> params = line.trimmed().split('=');
            if (params.size() == 2) {
                int portIndex = portDisplayNameToIndex(QString::fromLatin1(params[0]));
                QString portId = portIndexToId(portIndex);
                qint64 timestampMs = qnSyncTime->currentMSecsSinceEpoch();
                updateIOState(portId, params[1] == "active", timestampMs, false);
            }
        }
    }

    m_inputPortStateReader.reset();
}

QnPlAxisResource::IOMonitor* QnPlAxisResource::ioMonitorByHttpClient(nx_http::AsyncHttpClientPtr httpClient)
{
    if (m_inputIoMonitor.httpClient == httpClient)
        return &m_inputIoMonitor;
    else if (m_outputIoMonitor.httpClient == httpClient)
        return &m_outputIoMonitor;
    else
        return nullptr;
}

void QnPlAxisResource::onMonitorMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient )
{
    NX_ASSERT( httpClient );

    IOMonitor* ioMonitor = ioMonitorByHttpClient(httpClient);
    if (!ioMonitor)
        return;

    const nx_http::BufferType& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
    ioMonitor->contentParser->processData(msgBodyBuf);
}

void QnPlAxisResource::onMonitorConnectionClosed( nx_http::AsyncHttpClientPtr httpClient )
{
    if (getParentId() != commonModule()->moduleGUID() || !isInitialized())
        return;
    auto ioMonitor = ioMonitorByHttpClient(httpClient);
    if (!ioMonitor)
        return;
    ioMonitor->httpClient.reset();
    restartIOMonitorWithDelay();
}

void QnPlAxisResource::restartIOMonitorWithDelay()
{
    m_timer.start(
        kReconnectTimeout,
        [this]()
        {
            startInputPortMonitoring();
        }
    );
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
                changedParams.insert(paramNamePrefix + lit("Output.Active"), newValue.oDefaultState == Qn::IO_OpenCircuit ? lit("closed") : lit("open"));

            if (newValue.autoResetTimeoutMs != currentValue.autoResetTimeoutMs)
                changedParams.insert(paramNamePrefix + lit("Output.PulseTime"), QString::number(newValue.autoResetTimeoutMs));
        }
    }

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
    // it's error if can't read IO state for I/O module
    return !isIOModule();
};

void QnPlAxisResource::readPortIdLIst()
{
    QString value = getProperty(Qn::IO_PORT_DISPLAY_NAMES_PARAM_NAME);
    if (value.isEmpty())
        return;

    QnMutexLocker lock(&m_mutex);
    m_ioPortIdList.clear();
    for (const auto& portId: value.split(L','))
        m_ioPortIdList << portId;
}

bool QnPlAxisResource::initializeIOPorts( CLSimpleHTTPClient* const http )
{
    readPortIdLIst();

    QnIOPortDataList cameraPorts;
    if (!readPortSettings(http, cameraPorts))
        return ioPortErrorOccured();

    QnIOPortDataList savedPorts = getIOPorts();
    if (savedPorts.empty() && !cameraPorts.empty()) {
        QnMutexLocker lock(&m_mutex);
        m_ioPorts = cameraPorts;
    }
    else {
        auto mergedData = mergeIOSettings(cameraPorts, savedPorts);
        if (!savePortSettings(mergedData, cameraPorts))
            return ioPortErrorOccured();
        QnMutexLocker lock(&m_mutex);
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

void QnPlAxisResource::updateIOState(const QString& portId, bool isActive, qint64 timestampMs, bool overrideIfExist)
{
    QnMutexLocker lock(&m_mutex);
    QnIOStateData newValue(portId, isActive, timestampMs);
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
    {
        m_ioStates.push_back(newValue);
        if (!isActive)
            return;
    }

    for (const auto& port: m_ioPorts)
    {
        if (port.id == portId) {
            if (port.portType == Qn::PT_Input) {
                lock.unlock();
                emit cameraInput(
                    toSharedPointer(),
                    portId,
                    isActive,
                    timestampMs * 1000ll);
            }
            else if (port.portType == Qn::PT_Output) {
                lock.unlock();
                emit cameraOutput(
                    toSharedPointer(),
                    portId,
                    isActive,
                    timestampMs * 1000ll );
            }
            break;
        }
    }
}

QnIOStateDataList QnPlAxisResource::ioStates() const
{
    QnMutexLocker lock(&m_mutex);
    return m_ioStates;
}

void QnPlAxisResource::notificationReceived( const nx_http::ConstBufferRefType& notification )
{
    //1I:H, 1I:L, 1I:/, "1I:\"
    if (notification.isEmpty())
        return;
    NX_LOG( lit("Received notification %1 from %2").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logDEBUG1 );

    //notification
    size_t sepPos = nx::utils::find_first_of( notification, ":" );
    if (sepPos == nx::utils::BufferNpos || sepPos+1 >= notification.size())
    {
        NX_LOG( lit("Error parsing notification %1 from %2. Event type not found").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logINFO );
        return;
    }
    const char eventType = notification[sepPos+1];
    size_t portTypePos = nx::utils::find_first_not_of( notification, "0123456789" );
    if (portTypePos == nx::utils::BufferNpos)
    {
        NX_LOG( lit("Error parsing notification %1 from %2. Port type not found").arg(QLatin1String((QByteArray)notification)).arg(getUrl()), cl_logINFO );
        return;
    }


    QString portDisplayName = QString::fromLatin1(notification.mid(0, sepPos));
    int portIndex = portDisplayNameToIndex(portDisplayName);
    if (portIndex == -1) {
        NX_LOG( lit("Error parsing Axis notification message %1. Camera: %2").arg(QString::fromLatin1(notification)).arg(getUrl()), cl_logDEBUG1 );
        return;
    }
    QString portId = portIndexToId(portIndex);
    const char portType = notification[portTypePos];
    NX_LOG( lit("%1 port %2 changed its state to %3. Camera %4").
        arg(QLatin1String(portType == 'I' ? "Input" : "Output")).arg(portId).arg(QLatin1String(eventType == '/' || eventType == 'H' ? "active" : "inactive")).arg(getUrl()), cl_logDEBUG1 );

    if (eventType != '/' && eventType != '\\' /*&& eventType != 'H' && eventType != 'L'*/)
        return; // skip unknown event

    bool isOnState = (eventType == '/') || (eventType == 'H');

    // Axis camera sends inversed output port state for 'grounded circuit' ports (seems like it sends physical state instead of logical state)
    {
        QnMutexLocker lock(&m_mutex);
        for (auto& port: m_ioPorts) {
            if (port.id == portId)
            {
                if (port.portType == Qn::PT_Output && port.oDefaultState == Qn::IO_GroundedCircuit)
                    isOnState = !isOnState;
                break;
            }
        }
    }

    qint64 timestampMs = qnSyncTime->currentMSecsSinceEpoch();
    updateIOState(portId, isOnState, timestampMs, true);
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
    if (hasFlags(Qn::foreigner) ||      //we do not own camera
        m_ioPorts.empty() )    //camera report no io
    {
        return false;
    }
    if (m_inputPortStateReader)
        return true; //< Already started

    //based on VAPIX Version 3 I/O Port API

    QAuthenticator auth = getAuth();
    QUrl requestUrl;
    requestUrl.setHost( getHostAddress() );
    requestUrl.setPort( QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT) );
    //preparing request
    requestUrl.setPath(lit("/axis-cgi/io/port.cgi"));
    QString requestQuery = lit("checkactive=");

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

    requestQuery += portList;
    requestUrl.setQuery(requestQuery);

    nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
    httpClient->bindToAioThread(m_timer.getAioThread());
    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &QnPlAxisResource::onCurrentIOStateResponseReceived,
        Qt::DirectConnection );
    httpClient->setTotalReconnectTries( nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES );
    httpClient->setUserName( auth.user() );
    httpClient->setUserPassword( auth.password() );

    httpClient->doGet(requestUrl);

    m_inputPortStateReader = std::move( httpClient );
    return true;
}

void QnPlAxisResource::updateIOSettings()
{
    const auto newValue = QJson::deserialized<QnIOPortDataList>(getProperty(Qn::IO_SETTINGS_PARAM_NAME).toUtf8());
    QnIOPortDataList prevValue;
    {
        QnMutexLocker lock(&m_mutex);
        if (newValue == m_ioPorts)
            return;
        prevValue = m_ioPorts;
    }

    stopInputPortMonitoringSync();
    if (savePortSettings(newValue, prevValue))
    {
        {
            QnMutexLocker lock(&m_mutex);
            m_ioPorts = newValue;
        }
        startInputPortMonitoringAsync( std::function<void(bool)>() );
    }
    else
        setStatus(Qn::Offline); // reinit
}

void QnPlAxisResource::at_propertyChanged(const QnResourcePtr & res, const QString & key)
{
    if (key == Qn::IO_SETTINGS_PARAM_NAME && res && !res->hasFlags(Qn::foreigner))
    {
        auto weakPtr = weakPointer();
        nx::utils::concurrent::run(
            QThreadPool::globalInstance(),
            [weakPtr]()
            {
                if (auto res = weakPtr.toStrongRef().dynamicCast<QnPlAxisResource>())
                    res->updateIOSettings();
            });
    }
}

QList<QnCameraAdvancedParameter> QnPlAxisResource::getParamsByIds(const QSet<QString>& ids) const
{
    QList<QnCameraAdvancedParameter> params;

    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& id: ids)
        {
            auto param = m_advancedParameters.getParameterById(id);
            params.append(param);
        }
    }

    return params;
}

QString QnPlAxisResource::getAdvancedParametersTemplate() const
{
    return lit("axis.xml");
}

bool QnPlAxisResource::loadAdvancedParametersTemplateFromFile(QnCameraAdvancedParams& params, const QString& templateFilename)
{
    QFile paramsTemplateFile(templateFilename);

#ifdef _DEBUG
    QnCameraAdvacedParamsXmlParser::validateXml(&paramsTemplateFile);
#endif
    bool result = QnCameraAdvacedParamsXmlParser::readXml(&paramsTemplateFile, params);
#ifdef _DEBUG
    NX_ASSERT(result, lm("Error while parsing xml: %1").arg(templateFilename));
#endif
    return result;
}

QSet<QString> QnPlAxisResource::calculateSupportedAdvancedParameters(const QnCameraAdvancedParams& allParams)
{
    QSet<QString> supported;
    QList<QnCameraAdvancedParameter> paramList;
    auto paramIds = allParams.allParameterIds();
    bool success = true;

    for (const auto& paramId: paramIds)
    {
        auto param = allParams.getParameterById(paramId);
        if (!isMaintenanceParam(param))
            paramList.push_back(param);
        else
            supported.insert(paramId);
    }

    auto queries = buildGetParamsQueries(paramList);
    auto response = executeParamsQueries(queries, success);

    for (const auto& paramId: paramIds)
    {
        if (response.contains(paramId))
            supported.insert(paramId);
    }

    return supported;
}

void QnPlAxisResource::fetchAndSetAdvancedParameters()
{
    auto templateFile = getAdvancedParametersTemplate();
    QnCameraAdvancedParams params;
    if (!loadAdvancedParametersTemplateFromFile(
            params,
            lit(":/camera_advanced_params/") + templateFile))
    {
        return;
    }

    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    auto overloads = resData.value<std::vector<QnCameraAdvancedParameterOverload>>(
                Qn::ADVANCED_PARAMETER_OVERLOADS_PARAM_NAME);

    params.applyOverloads(overloads);

    auto supportedParams = calculateSupportedAdvancedParameters(params);

    auto filteredParams = params.filtered(supportedParams);

    {
        QnMutexLocker lock(&m_mutex);
        m_advancedParameters = filteredParams;
    }

    QnCameraAdvancedParamsReader::setParamsToResource(this->toSharedPointer(), filteredParams);
}

bool QnPlAxisResource::isMaintenanceParam(const QnCameraAdvancedParameter &param) const
{
    return  param.dataType == QnCameraAdvancedParameter::DataType::Button;
}

QMap<QString, QString> QnPlAxisResource::executeParamsQueries(const QSet<QString> &queries, bool &isSuccessful) const
{
    QMap<QString, QString> result;
    CLHttpStatus status;
    isSuccessful = true;

    CLSimpleHTTPClient httpClient(
        getHostAddress(),
        QUrl(getUrl()).port(DEFAULT_AXIS_API_PORT),
        getNetworkTimeout(),
        getAuth());

    for (const auto& query: queries)
    {
        if (QnResource::isStopping())
            break;

        status = httpClient.doGET(query);
        if ( status == CL_HTTP_SUCCESS )
        {
            QByteArray body;
            httpClient.readAll( body );

            if (body.startsWith("OK"))
                continue;

            for (const QByteArray& line: body.split('\n'))
            {
                const auto& paramItems = line.split('=');
                if( paramItems.size() == 2)
                    result[paramItems[0]] = paramItems[1];
            }
        }
        else
        {
            isSuccessful = false;
            NX_LOG(lit("Failed to execute params query. Param: %1, device: %2 (%3), result: %4")
                .arg(query)
                .arg(getModel())
                .arg(getHostAddress())
                .arg(::toString(status)),
                cl_logDEBUG2);
        }
    }
    return result;
}

QMap<QString, QString>  QnPlAxisResource::executeParamsQueries(
    const QString &query,
    bool &isSuccessful) const
{
    QSet<QString> queries;
    queries.insert(query);
    return executeParamsQueries(queries, isSuccessful);
}

QnCameraAdvancedParamValueList QnPlAxisResource::parseParamsQueriesResult(
    const QMap<QString, QString> &queriesResult,
    const QList<QnCameraAdvancedParameter> &params) const
{
    QnCameraAdvancedParamValueList result;
    for (const auto& param: params)
    {
        if (queriesResult.contains(param.id))
        {
            auto paramValue = param.dataType == QnCameraAdvancedParameter::DataType::Enumeration
                ? param.fromInternalRange(queriesResult[param.id])
                : queriesResult[param.id];

            result.append(QnCameraAdvancedParamValue(param.id, paramValue));
        }
    }

    return result;
}

QSet<QString> QnPlAxisResource::buildGetParamsQueries(const QList<QnCameraAdvancedParameter> &params) const
{
    QSet<QString> result;
    for (const auto& param: params)
    {
        if (isMaintenanceParam(param))
            continue;

        auto paramPath = param.id.left(param.id.lastIndexOf('.'));
        result.insert(lit("axis-cgi/admin/param.cgi?action=list&group=") + paramPath);
    }

    return result;
}

QString QnPlAxisResource::buildSetParamsQuery(const QnCameraAdvancedParamValueList &params) const
{
    const QString kPrefix = lit("axis-cgi/admin/param.cgi?");
    QUrlQuery query;

    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& paramIdAndValue: params)
        {
            auto param = m_advancedParameters.getParameterById(paramIdAndValue.id);
            if (isMaintenanceParam(param))
                continue;

            auto paramValue = paramIdAndValue.value;
            paramValue = param.dataType == QnCameraAdvancedParameter::DataType::Enumeration
                ? param.toInternalRange(paramValue)
                : paramValue;

            query.addQueryItem(paramIdAndValue.id, paramValue);
        }
    }

    if (query.isEmpty())
        return QString();

    query.addQueryItem(lit("action"), lit("update"));
    return kPrefix + query.toString();
}

QString QnPlAxisResource::buildMaintenanceQuery(const QnCameraAdvancedParamValueList& params) const
{
    QString query = lit("axis-cgi/admin/");

    {
        QnMutexLocker lock(&m_mutex);
        for (const auto& paramIdAndValue: params)
        {
            auto param = m_advancedParameters.getParameterById(paramIdAndValue.id);
            if (isMaintenanceParam(param))
                return query + param.readCmd;
        }
    }
    return QString();
}

bool QnPlAxisResource::getParamPhysical(const QString &id, QString &value)
{
    QSet<QString> idList;
    QnCameraAdvancedParamValueList paramValueList;
    idList.insert(id);
    auto result = getParamsPhysical(idList, paramValueList);
    if (!paramValueList.isEmpty())
        value = paramValueList.first().value;

    return result;
}

bool QnPlAxisResource::getParamsPhysical(const QSet<QString> &idList, QnCameraAdvancedParamValueList& result)
{
    bool success = true;
    const auto params = getParamsByIds(idList);
    const auto queries = buildGetParamsQueries(params);

    const auto queriesResults = executeParamsQueries(queries, success);

    result = parseParamsQueriesResult(queriesResults, params);

    return result.size() == idList.size();

}

bool QnPlAxisResource::setParamPhysical(const QString &id, const QString& value)
{
    QnCameraAdvancedParamValueList inputParamList;
    QnCameraAdvancedParamValueList resParamList;
    inputParamList.append({ id, value });
    return setParamsPhysical(inputParamList, resParamList);
}

bool QnPlAxisResource::setParamsPhysical(const QnCameraAdvancedParamValueList &values, QnCameraAdvancedParamValueList &result)
{
    bool success;
    const auto query = buildSetParamsQuery(values);
    const auto maintenanceQuery = buildMaintenanceQuery(values);

    result.clear();

    if (!query.isEmpty())
        executeParamsQueries(query, success);

    if (!maintenanceQuery.isEmpty())
        executeParamsQueries(maintenanceQuery, success);

    if (success)
        result.append(values);

    return success;
}

#endif // #ifdef ENABLE_AXIS
