#ifdef ENABLE_ARECONT

#ifdef _WIN32
#  include <winsock2.h>
#endif

#if defined(QT_LINUXBASE)
#  include <arpa/inet.h>
#endif

#include <memory>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <nx/utils/concurrent.h>
#include <nx/utils/log/log.h>
#include <utils/common/synctime.h>
#include <nx/utils/timer_manager.h>
#include <nx/network/http/http_client.h>
#include <nx/network/nettools.h>
#include <nx/network/ping.h>

#include "core/resource/resource_command_processor.h"
#include "core/resource/resource_command.h"
#include "core/resource_management/resource_data_pool.h"

#include "av_resource.h"
#include "av_panoramic.h"
#include "av_singesensor.h"
#include <common/static_common_module.h>


const QString QnPlAreconVisionResource::MANUFACTURE(lit("ArecontVision"));
#define MAX_RESPONSE_LEN (4*1024)


QnPlAreconVisionResource::QnPlAreconVisionResource()
    : m_totalMdZones(64),
      m_zoneSite(8),
      m_channelCount(0),
      m_prevMotionChannel(0),
      m_dualsensor(false),
      m_inputPortState(false),
      m_advancedParamitersProvider{this}
{
    setVendor(lit("ArecontVision"));
}

bool QnPlAreconVisionResource::isPanoramic() const
{
    return const_cast<QnPlAreconVisionResource*>(this)->getVideoLayout(0)->channelCount() > 1;
}

bool QnPlAreconVisionResource::isDualSensor() const
{
    const QString model = getModel();
    return model.contains(lit("3130")) || model.contains(lit("3135")); // TODO: #Elric #vasilenko move to json?
}

CLHttpStatus QnPlAreconVisionResource::getRegister(int page, int num, int& val)
{
    QString req;
    QTextStream(&req) << "getreg?page=" << page << "&reg=" << num;

    QUrl devUrl(getUrl());
    CLSimpleHTTPClient http(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());

    CLHttpStatus result = http.doGET(req);

    if (result!=CL_HTTP_SUCCESS)
        return result;

    QByteArray arr;
    http.readAll(arr);
    int index = arr.indexOf('=');
    if (index==-1)
        return CL_TRANSPORT_ERROR;

    QByteArray cnum = arr.mid(index+1);
    val = cnum.toInt();

    return CL_HTTP_SUCCESS;

}

CLHttpStatus QnPlAreconVisionResource::setRegister(int page, int num, int val)
{
    QString req;
    QTextStream(&req) << "setreg?page=" << page << "&reg=" << num << "&val=" << val;
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient http(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());

    CLHttpStatus result = http.doGET(req);

    return result;

}

class QnPlArecontResourceSetRegCommand : public QnResourceCommand
{
public:
    QnPlArecontResourceSetRegCommand(const QnResourcePtr& res, int page, int reg, int val):
      QnResourceCommand(res),
          m_page(page),
          m_reg(reg),
          m_val(val)
      {}


      bool execute()
      {
          if (!isConnectedToTheResource())
              return false;

          return getResource().dynamicCast<QnPlAreconVisionResource>()->setRegister(m_page,m_reg,m_val);
      }
private:
    int m_page;
    int m_reg;
    int m_val;
};

typedef std::shared_ptr<QnPlArecontResourceSetRegCommand> QnPlArecontResourceSetRegCommandPtr;

CLHttpStatus QnPlAreconVisionResource::setRegister_asynch(int page, int num, int val)
{
    QnPlArecontResourceSetRegCommandPtr command ( new QnPlArecontResourceSetRegCommand(toSharedPointer(), page, num, val) );
    addCommandToProc(command);
    return CL_HTTP_SUCCESS;

}

void QnPlAreconVisionResource::setHostAddress(const QString& hostAddr)
{
    QnNetworkResource::setHostAddress(hostAddr);
}

bool QnPlAreconVisionResource::ping()
{
    nx::utils::concurrent::Future<bool> result(1);
    checkIfOnlineAsync(
        [&result]( bool onlineOrNot ) {
            result.setResultAt(0, onlineOrNot);
        } );
    result.waitForFinished();
    return result.resultAt(0);
}

void QnPlAreconVisionResource::checkIfOnlineAsync( std::function<void(bool)> completionHandler )
{
    //checking that camera is alive and on its place
    const QString& urlStr = getUrl();
    QUrl url = QUrl(urlStr);
    if( url.host().isEmpty() )
    {
        //url is just IP address?
        url.setScheme( lit("http") );
        url.setHost( urlStr );
    }
    url.setPath(lit("/get"));
    url.setQuery(lit("mac"));
    QAuthenticator auth = getAuth();
    url.setUserName( auth.user() );
    url.setPassword( auth.password() );

    nx_http::AsyncHttpClientPtr httpClientCaptured = nx_http::AsyncHttpClient::create();
    httpClientCaptured->setResponseReadTimeoutMs(getNetworkTimeout());

    const QnMacAddress cameraMAC = getMAC();
    auto httpReqCompletionHandler = [httpClientCaptured, cameraMAC, completionHandler]
        ( const nx_http::AsyncHttpClientPtr& httpClient ) mutable
    {
        httpClientCaptured->disconnect( nullptr, (const char*)nullptr );
        httpClientCaptured.reset();

        auto completionHandlerLocal = std::move( completionHandler );
        if( (httpClient->failed()) ||
            (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok) )
        {
            completionHandlerLocal( false );
            return;
        }
        nx_http::BufferType msgBody = httpClient->fetchMessageBodyBuffer();
        const int sepIndex = msgBody.indexOf('=');
        if( sepIndex == -1 )
        {
            completionHandlerLocal( false );
            return;
        }
        const QByteArray& mac = msgBody.mid( sepIndex+1 );
        completionHandlerLocal( cameraMAC == QnMacAddress(mac) );
    };
    connect( httpClientCaptured.get(), &nx_http::AsyncHttpClient::done,
             this, httpReqCompletionHandler,
             Qt::DirectConnection );

    httpClientCaptured->doGet( url );
}

CameraDiagnostics::Result QnPlAreconVisionResource::initializeCameraDriver()
{
    QString maxSensorWidth;
    QString maxSensorHeight;
    {
        // TODO: #Elric is this needed? This was a call to setCroppingPhysical
        maxSensorWidth = getProperty(lit("MaxSensorWidth"));
        maxSensorHeight = getProperty(lit("MaxSensorHeight"));

        setApiParameter(lit("sensorleft"), QString::number(0));
        setApiParameter(lit("sensortop"), QString::number(0));
        setApiParameter(lit("sensorwidth"), maxSensorWidth);
        setApiParameter(lit("sensorheight"), maxSensorHeight);
    }

    QString firmwareVersion;
    if (!getApiParameter(lit("fwversion"), firmwareVersion))
        return CameraDiagnostics::RequestFailedResult(lit("Firmware version"), lit("unknown"));

    QString procVersion;
    if (!getApiParameter(lit("procversion"), procVersion))
        return CameraDiagnostics::RequestFailedResult(lit("Image engine"), lit("unknown"));

    QString netVersion;
    if (!getApiParameter(lit("netversion"), netVersion))
        return CameraDiagnostics::RequestFailedResult(lit("Net version"), lit("unknown"));

    //if (!getDescription())
    //    return;

    setRegister(3, 21, 20); // sets I frame frequency to 1/20


    if (!setApiParameter(lit("motiondetect"), lit("on"))) // enables motion detection;
        return CameraDiagnostics::RequestFailedResult(lit("Enable motion detection"), lit("unknown"));

    // check if we've got 1024 zones
    QString totalZones = QString::number(1024);
    setApiParameter(lit("mdtotalzones"), totalZones); // try to set total zones to 64; new cams support it
    if (!getApiParameter(lit("mdtotalzones"), totalZones))
        return CameraDiagnostics::RequestFailedResult(lit("TotalZones"), lit("unknown"));

    if (totalZones.toInt() == 1024)
        m_totalMdZones = 1024;

    // lets set zone size
    //one zone - 32x32 pixels; zone sizes are 1-15

    int optimal_zone_size_pixels = maxSensorWidth.toInt() / (m_totalMdZones == 64 ? 8 : 32);

    int zone_size = (optimal_zone_size_pixels%32) ? (optimal_zone_size_pixels/32 + 1) : optimal_zone_size_pixels/32;

    if (zone_size>15)
        zone_size = 15;

    if (zone_size<1)
        zone_size = 1;

    if (qnStaticCommon->dataPool()->data(toSharedPointer(this)).value<bool>(lit("hasRelayInput"), true))
        setCameraCapability(Qn::RelayInputCapability, true);
    if (qnStaticCommon->dataPool()->data(toSharedPointer(this)).value<bool>(lit("hasRelayOutput"), true))
        setCameraCapability(Qn::RelayOutputCapability, true);

    setFirmware(firmwareVersion);
    saveParams();

    setApiParameter(lit("mdzonesize"), QString::number(zone_size));
    m_zoneSite = zone_size;
    setMotionMaskPhysical(0);

    m_channelCount = getVideoLayout(0)->channelCount();
    m_dualsensor = isDualSensor();

    return CameraDiagnostics::NoErrorResult();
}

int QnPlAreconVisionResource::getZoneSite() const
{
    return m_zoneSite;
}

QString QnPlAreconVisionResource::getDriverName() const
{
    return MANUFACTURE;
}

Qn::StreamQuality QnPlAreconVisionResource::getBestQualityForSuchOnScreenSize(const QSize& /*size*/) const
{
    return Qn::QualityNormal;
}

QImage QnPlAreconVisionResource::getImage(int /*channnel*/, QDateTime /*time*/, Qn::StreamQuality /*quality*/) const
{
    return QImage();
}

void QnPlAreconVisionResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

bool QnPlAreconVisionResource::setRelayOutputState(
    const QString& /*ouputID*/,
    bool activate,
    unsigned int autoResetTimeoutMS)
{
    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(getHostAddress());
    url.setPort(QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT));
    url.setPath(lit("/set"));
    url.setQuery(lit("auxout=%1").arg(activate ? lit("on") : lit("off")));

    QAuthenticator auth = getAuth();

    url.setUserName(auth.user());
    url.setPassword(auth.password());

    const auto activateWithAutoResetDoneHandler =
        [autoResetTimeoutMS, url](
            SystemError::ErrorCode errorCode,
            int statusCode,
            nx_http::BufferType)
        {
            if (errorCode != SystemError::noError ||
                statusCode != nx_http::StatusCode::ok)
            {
                return;
            }

            //scheduling auto-reset
            nx::utils::TimerManager::instance()->addTimer(
                [url](qint64){
                    auto resetOutputUrl = url;
                    resetOutputUrl.setPath(lit("/set"));
                    resetOutputUrl.setQuery(lit("auxout=off"));
                    nx_http::downloadFileAsync(
                        resetOutputUrl,
                        [](SystemError::ErrorCode, int, nx_http::BufferType){});
                },
                std::chrono::milliseconds(autoResetTimeoutMS));
        };

    const auto emptyOutputDoneHandler = [](SystemError::ErrorCode, int, nx_http::BufferType) {};

    if (activate && (autoResetTimeoutMS > 0))
        nx_http::downloadFileAsync(url, activateWithAutoResetDoneHandler);
    else
        nx_http::downloadFileAsync(url, emptyOutputDoneHandler);
    return true;
}

int QnPlAreconVisionResource::totalMdZones() const
{
    return m_totalMdZones;
}

bool QnPlAreconVisionResource::isH264() const
{
    return getProperty(lit("Codec")) == lit("H.264");
}

QnMetaDataV1Ptr QnPlAreconVisionResource::getCameraMetadata()
{
    QnMetaDataV1Ptr motion(new QnMetaDataV1());
    QString mdresult;
    if (m_channelCount == 1)
    {
        if (!getApiParameter(QLatin1String("mdresult"), mdresult))
            return QnMetaDataV1Ptr(0);
    }
    else
    {
        if (!getParamPhysical2(m_prevMotionChannel+1, QLatin1String("mdresult"), mdresult))
            return QnMetaDataV1Ptr(0);
        motion->channelNumber = m_prevMotionChannel;
        ++m_prevMotionChannel;
        if (m_prevMotionChannel == m_channelCount)
            m_prevMotionChannel = 0;
    }

    if (mdresult == lit("no motion"))
        return motion; // no motion detected


    int zones = totalMdZones() == 1024 ? 32 : 8;

    QStringList md = mdresult.split(L' ', QString::SkipEmptyParts);
    if (md.size() < zones*zones)
        return QnMetaDataV1Ptr(0);

    int pixelZoneSize = getZoneSite() * 32;
    if (pixelZoneSize == 0)
        return QnMetaDataV1Ptr(0);

    QVariant maxSensorWidth = getProperty(lit("MaxSensorWidth"));
    QVariant maxSensorHight = getProperty(lit("MaxSensorHeight"));

    QRect imageRect(0, 0, maxSensorWidth.toInt(), maxSensorHight.toInt());
    QRect zeroZoneRect(0, 0, pixelZoneSize, pixelZoneSize);

    for (int x = 0; x < zones; ++x)
    {
        for (int y = 0; y < zones; ++y)
        {
            int index = y*zones + x;
            QString m = md.at(index);


            if (m == lit("00") || m == lit("0"))
                continue;

            QRect currZoneRect = zeroZoneRect.translated(x*pixelZoneSize, y*pixelZoneSize);

            motion->mapMotion(imageRect, currZoneRect);

        }
    }

    //motion->m_duration = META_DATA_DURATION_MS * 1000 ;
    motion->m_duration = 1000 * 1000 * 1000; // 1000 sec
    return motion;
}

QString QnPlAreconVisionResource::generateRequestString(
    const QHash<QByteArray, QVariant>& streamParams,
    bool h264,
    bool resolutionFULL,
    bool blackWhite,
    int* const outQuality,
    QSize* const outResolution)
{
    QString request;

    int left;
    int top;
    int right;
    int bottom;

    int width;
    int height;

    int quality = 0;

    int streamID;

    int bitrate = 0;

    if (!streamParams.contains("Quality") || !streamParams.contains("resolution") ||
        !streamParams.contains("image_left") || !streamParams.contains("image_top") ||
        !streamParams.contains("image_right") || !streamParams.contains("image_bottom") ||
        (h264 && !streamParams.contains("streamID")))
    {
        NX_LOG("Error!!! parameter is missing in stream params.", cl_logERROR);
        //return QnAbstractMediaDataPtr(0);
    }

    // =========
    left = streamParams.value("image_left").toInt();
    top = streamParams.value("image_top").toInt();
    right = streamParams.value("image_right").toInt();
    bottom = streamParams.value("image_bottom").toInt();

    if (m_dualsensor && blackWhite) //3130 || 3135
    {
        right = right / 3 * 2;
        bottom = bottom / 3 * 2;

        right = right / 32 * 32;
        bottom = bottom / 16 * 16;

        right = qMin(1280, right);
        bottom = qMin(1024, bottom);

    }

    //right = 1280;
    //bottom = 1024;

    //right/=2;
    //bottom/=2;

    width = right - left;
    height = bottom - top;

    quality = streamParams.value("Quality").toInt();
    //quality = getQuality();

    streamID = 0;
    if (h264)
    {
        streamID = streamParams.value("streamID").toInt();
        //bitrate = streamParams.value("Bitrate").toInt();
        bitrate = getProperty(lit("Bitrate")).toInt();
    }
    // =========

    if (h264)
        quality = 37 - quality; // for H.264 it's not quality; it's qp

    if (!h264)
        request += QLatin1String("image");
    else
        request += QLatin1String("h264");

    request += QLatin1String("?res=");
    if (resolutionFULL)
        request += QLatin1String("full");
    else
        request += QLatin1String("half");

    request += QLatin1String(";x0=") + QString::number(left)
        + QLatin1String(";y0=") + QString::number(top)
        + QLatin1String(";x1=") + QString::number(right)
        + QLatin1String(";y1=") + QString::number(bottom);

    if (!h264)
        request += QLatin1String(";quality=");
    else
        request += QLatin1String(";qp=");

    request += QString::number(quality) + QLatin1String(";doublescan=0") + QLatin1String(";ssn=") + QString::number(streamID);

    //h264?res=full;x0=0;y0=0;x1=1600;y1=1184;qp=27;doublescan=0;iframe=0;ssn=574;netasciiblksize1450
    //request = "image?res=full;x0=0;y0=0;x1=800;y1=600;quality=10;doublescan=0;ssn=4184;";

    if (h264)
    {
        if (bitrate)
            request += QLatin1String("bitrate=") + QString::number(bitrate) + QLatin1Char(';');
    }

    if (outQuality)
        *outQuality = quality;
    if (outResolution)
        *outResolution = QSize(width, height);

    return request;
}

// ===============================================================================================================================
bool QnPlAreconVisionResource::getApiParameter(const QString &id, QString &value) {
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());

    QString request = lit("get?") + id;

    CLHttpStatus status = connection.doGET(request);
    if (status == CL_HTTP_AUTH_REQUIRED && !getId().isNull())
        setStatus(Qn::Unauthorized);

    if (status != CL_HTTP_SUCCESS)
        return false;


    QByteArray response;
    connection.readAll(response);
    int index = response.indexOf('=');
    if (index==-1)
        return false;

    QByteArray rarray = response.mid(index+1);

    value = QLatin1String(rarray.data());

    return true;
}

bool QnPlAreconVisionResource::setApiParameter(const QString &id, const QString &value) {
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());

    QString request = lit("set?") + id;
    request += QLatin1Char('=') + value;

    CLHttpStatus status = connection.doGET(request);
    if (status != CL_HTTP_SUCCESS)
        status = connection.doGET(request);

    if (CL_HTTP_SUCCESS == status)
        return true;

    if (CL_HTTP_AUTH_REQUIRED == status)
    {
        setStatus(Qn::Unauthorized);
        return false;
    }

    return false;
}

QnPlAreconVisionResource* QnPlAreconVisionResource::createResourceByName(const QString &name)
{
    QnUuid rt = qnResTypePool->getLikeResourceTypeId(MANUFACTURE, name);
    if (rt.isNull())
    {
        if ( name.left(2).toLower() == lit("av") )
        {
            QString new_name = name.mid(2);
            rt = qnResTypePool->getLikeResourceTypeId(MANUFACTURE, new_name);
            if (rt.isNull())
            {
                NX_LOG( lit("Unsupported AV resource found: %1").arg(name), cl_logERROR);
                return 0;
            }
        }
    }

    return createResourceByTypeId(rt);

}

QnPlAreconVisionResource* QnPlAreconVisionResource::createResourceByTypeId(QnUuid rt)
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(rt);

    if (resourceType.isNull() || (resourceType->getManufacture() != MANUFACTURE))
    {
        NX_LOG( lit("Can't create AV Resource. Resource type is invalid. %1").arg(rt.toString()), cl_logERROR);
        return 0;
    }

    QnPlAreconVisionResource* res = 0;

    if (isPanoramic(resourceType))
        res = new QnArecontPanoramicResource(resourceType->getName());
    else
        res = new CLArecontSingleSensorResource(resourceType->getName());

    res->setTypeId(rt);

    return res;
}

bool QnPlAreconVisionResource::isPanoramic(QnResourceTypePtr resType)
{
    QString layoutStr = resType->defaultValue(Qn::VIDEO_LAYOUT_PARAM_NAME);
    return !layoutStr.isEmpty() && QnCustomResourceVideoLayout::fromString(layoutStr)->channelCount() > 1;
};

QnAbstractStreamDataProvider* QnPlAreconVisionResource::createLiveDataProvider()
{
    NX_ASSERT(false, Q_FUNC_INFO, "QnPlAreconVisionResource is abstract.");
    return 0;
}

void QnPlAreconVisionResource::setMotionMaskPhysical(int channel)
{
    if (channel != 0)
        return; // motion info used always once even for multisensor cameras

    static int sensToLevelThreshold[10] =
    {
        31, // 0 - aka mask really filtered by server always
        31, // 1
        25, // 2
        19, // 3
        14, // 4
        10, // 5
        7,  // 6
        5,  // 7
        3,  // 8
        2   // 9
    };

    QnMotionRegion region = getMotionRegion(0);
    for (int sens = 1; sens < QnMotionRegion::kSensitivityLevelCount; ++sens)
    {

        if (!region.getRegionBySens(sens).isEmpty())
        {
            setApiParameter(lit("mdlevelthreshold"), QString::number(sensToLevelThreshold[sens]));
            break; // only 1 sensitivity for all frame is supported
        }
    }
}

bool QnPlAreconVisionResource::startInputPortMonitoringAsync(std::function<void(bool)>&& completionHandler)
{
    QUrl url;
    url.setScheme(lit("http"));
    url.setHost(getHostAddress());
    url.setPort(QUrl(getUrl()).port(nx_http::DEFAULT_HTTP_PORT));
    url.setPath(lit("/get"));
    url.setQuery(lit("auxin"));

    QAuthenticator auth = getAuth();

    url.setUserName(auth.user());
    url.setPassword(auth.password());

    m_relayInputClient = nx_http::AsyncHttpClient::create();
    connect(m_relayInputClient.get(), &nx_http::AsyncHttpClient::done,
            this,
            [this, completionHandler](nx_http::AsyncHttpClientPtr client) {
                if (completionHandler)
                    completionHandler(
                        client->response() &&
                        client->response()->statusLine.statusCode == nx_http::StatusCode::ok);
                inputPortStateRequestDone(std::move(client));
            },
            Qt::DirectConnection);
    m_relayInputClient->doGet(url);
    return true;
}

void QnPlAreconVisionResource::stopInputPortMonitoringAsync()
{
    m_relayInputClient.reset();
}

void QnPlAreconVisionResource::inputPortStateRequestDone(nx_http::AsyncHttpClientPtr client)
{
    static const unsigned int INPUT_PORT_STATE_CHECK_TIMEOUT_MS = 1000;

    bool portEnabled = false;
    if (client->failed() ||
        client->response() == nullptr ||
        client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        portEnabled = false;
    }
    else
    {
        const auto msg = client->fetchMessageBodyBuffer();
        portEnabled = msg.indexOf("auxin=on") != -1;
    }

    if (m_inputPortState != portEnabled)
    {
        m_inputPortState = portEnabled;
        emit cameraInput(
            toSharedPointer(),
            lit("1"),
            m_inputPortState,
            qnSyncTime->currentMSecsSinceEpoch() * 1000ll);
    }

    const auto url = client->url();
    //scheduling next HTTP request
    client->socket()->registerTimer(
        INPUT_PORT_STATE_CHECK_TIMEOUT_MS,
        [url, this](){
            m_relayInputClient->doGet(url);
        });
}

bool QnPlAreconVisionResource::isRTSPSupported() const
{
    auto resData = qnStaticCommon->dataPool()->data(toSharedPointer(this));
    auto arecontRtspIsAllowed = qnGlobalSettings->arecontRtspEnabled();
    auto cameraSupportsH264 = isH264();
    auto cameraSupportsRtsp = resData.value<bool>(lit("isRTSPSupported"), true);
    auto rtspIsForcedOnCamera = resData.value<bool>(lit("forceRtspSupport"), false);

    return arecontRtspIsAllowed
        && ((cameraSupportsH264 && cameraSupportsRtsp) || rtspIsForcedOnCamera);
}

QnCameraAdvancedParams QnPlAreconVisionResource::AvParamitersProvider::descriptions()
{
    return {};
}

QnCameraAdvancedParamValueMap QnPlAreconVisionResource::AvParamitersProvider::get(const QSet<QString>& ids)
{
    QnCameraAdvancedParamValueMap result;
    for (const auto id: ids)
    {
        QString value;
        if (m_resource->getApiParameter(id, value))
            result.insert(id, value);
    }

    return result;
}

QSet<QString> QnPlAreconVisionResource::AvParamitersProvider::set(const QnCameraAdvancedParamValueMap& values)
{
    QSet<QString> result;
    for (const auto id: values)
    {
        QString value;
        if (m_resource->setApiParameter(id, values.value(id)))
            result.insert(id);
    }

    return result;
}

std::vector<QnPlAreconVisionResource::AdvancedParametersProvider*>
    QnPlAreconVisionResource::advancedParametersProviders()
{
    return {&m_advancedParamitersProvider};
}

bool QnPlAreconVisionResource::getParamPhysical2(int channel, const QString& name, QString &val)
{
    m_mutex.lock();
    m_mutex.unlock();
    QUrl devUrl(getUrl());

    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());
    QString request = QLatin1String("get") + QString::number(channel) + QLatin1String("?") + name;

    CLHttpStatus status = connection.doGET(request);
    if (status == CL_HTTP_AUTH_REQUIRED)
        setStatus(Qn::Unauthorized);

    if (status != CL_HTTP_SUCCESS)
        return false;


    QByteArray response;
    connection.readAll(response);
    int index = response.indexOf('=');
    if (index==-1)
        return false;

    val = QLatin1String(response.mid(index+1));

    return true;
}

#endif
