#ifdef ENABLE_ARECONT

#ifdef _WIN32
#  include <winsock2.h>
#endif

#if defined(QT_LINUXBASE)
#  include <arpa/inet.h>
#endif

#include <memory>

#include <utils/common/concurrent.h>
#include <utils/common/log.h>
#include <utils/network/http/httpclient.h>
#include <utils/network/nettools.h>
#include <utils/network/ping.h>

#include "core/resource/resource_command_processor.h"
#include "core/resource/resource_command.h"

#include "av_resource.h"
#include "av_panoramic.h"
#include "av_singesensor.h"


const QString QnPlAreconVisionResource::MANUFACTURE(lit("ArecontVision"));
#define MAX_RESPONSE_LEN (4*1024)


QnPlAreconVisionResource::QnPlAreconVisionResource()
    : m_totalMdZones(64),
      m_zoneSite(8)
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

bool QnPlAreconVisionResource::unknownResource() const
{
    return isAbstractResource();
}

QnResourcePtr QnPlAreconVisionResource::updateResource()
{
    QString model;
    QString model_release;

    if (!getParamPhysical(lit("model"), model))
        return QnNetworkResourcePtr(0);

    if (!getParamPhysical(lit("model=releasename"), model_release))
        return QnNetworkResourcePtr(0);

    if (model_release != model) {
        //this camera supports release name
        model = model_release;
    }
    else
    {
        //old camera; does not support release name; but must support fullname
        if (getParamPhysical(lit("model=fullname"), model_release))
            model = model_release;
    }

    QnNetworkResourcePtr result(createResourceByName(model));
    if (result)
    {
        result->setName(model);
        result->setHostAddress(getHostAddress());
        (result.dynamicCast<QnPlAreconVisionResource>())->setModel(model);
        result->setMAC(getMAC());
        result->setId(getId());
        result->setFlags(flags());
    }
    else
    {
        NX_LOG( lit("Found unknown resource! %1").arg(model), cl_logWARNING);
    }

    return result;
}

bool QnPlAreconVisionResource::ping()
{
    QnConcurrent::QnFuture<bool> result(1);
    if( !checkIfOnlineAsync( [&result]( bool onlineOrNot ) {
            result.setResultAt(0, onlineOrNot); } ) )
        return false;
    result.waitForFinished();
    return result.resultAt(0);
}

bool QnPlAreconVisionResource::checkIfOnlineAsync( std::function<void(bool)>&& completionHandler )
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
    url.setPath( lit("/get?mac") );
    url.setUserName( getAuth().user() );
    url.setPassword( getAuth().password() );

    nx_http::AsyncHttpClientPtr httpClientCaptured = nx_http::AsyncHttpClient::create();
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

    if( !httpClientCaptured->doGet( url ) )
    {
        httpClientCaptured->disconnect( nullptr, (const char*)nullptr );
        return false;
    }
    return true;
}

CameraDiagnostics::Result QnPlAreconVisionResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    
    QString maxSensorWidth;
    QString maxSensorHeight;
    {
        // TODO: #Elric is this needed? This was a call to setCroppingPhysical
        maxSensorWidth = getProperty(lit("MaxSensorWidth"));
        maxSensorHeight = getProperty(lit("MaxSensorHeight"));

        setParamPhysicalAsync(lit("sensorleft"), QString::number(0));
        setParamPhysicalAsync(lit("sensortop"), QString::number(0));
        setParamPhysicalAsync(lit("sensorwidth"), maxSensorWidth);
        setParamPhysicalAsync(lit("sensorheight"), maxSensorHeight);
    }

    QString firmwareVersion;
    if (!getParamPhysical(lit("fwversion"), firmwareVersion))
        return CameraDiagnostics::RequestFailedResult(lit("Firmware version"), lit("unknown"));

    QString procVersion;
    if (!getParamPhysical(lit("procversion"), procVersion))
        return CameraDiagnostics::RequestFailedResult(lit("Image engine"), lit("unknown"));

    QString netVersion;
    if (!getParamPhysical(lit("netversion"), netVersion))
        return CameraDiagnostics::RequestFailedResult(lit("Net version"), lit("unknown"));

    //if (!getDescription())
    //    return;

    setRegister(3, 21, 20); // sets I frame frequency to 1/20


    if (!setParamPhysical(lit("motiondetect"), lit("on"))) // enables motion detection;
        return CameraDiagnostics::RequestFailedResult(lit("Enable motion detection"), lit("unknown"));

    // check if we've got 1024 zones
    QString totalZones = QString::number(1024);
    setParamPhysical(lit("mdtotalzones"), totalZones); // try to set total zones to 64; new cams support it
    if (!getParamPhysical(lit("mdtotalzones"), totalZones))
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

    setFirmware(firmwareVersion);
    saveParams();

    setParamPhysical(lit("mdzonesize"), QString::number(zone_size));
    m_zoneSite = zone_size;
    setMotionMaskPhysical(0);

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

int QnPlAreconVisionResource::totalMdZones() const
{
    return m_totalMdZones;
}

bool QnPlAreconVisionResource::isH264() const
{
    return getProperty(lit("Codec")) == lit("H.264");
}

//===============================================================================================================================
bool QnPlAreconVisionResource::getParamPhysical(const QString &id, QString &value) {
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

bool QnPlAreconVisionResource::setParamPhysical(const QString &id, const QString &value) {
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
            if (!rt.isNull())
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
    Q_ASSERT_X(false, Q_FUNC_INFO, "QnPlAreconVisionResource is abstract.");
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
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        
        if (!region.getRegionBySens(sens).isEmpty())
        {
            setParamPhysicalAsync(lit("mdlevelthreshold"), QString::number(sensToLevelThreshold[sens]));
            break; // only 1 sensitivity for all frame is supported
        }
    }
}

bool QnPlAreconVisionResource::isAbstractResource() const
{
    QnUuid baseTypeId = qnResTypePool->getResourceTypeId(QnPlAreconVisionResource::MANUFACTURE, QLatin1String("ArecontVision_Abstract"));
    return getTypeId() == baseTypeId;
}

#endif
