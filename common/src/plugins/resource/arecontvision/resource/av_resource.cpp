#ifdef ENABLE_ARECONT

#ifdef _WIN32
#  include <winsock2.h>
#endif

#if defined(QT_LINUXBASE)
#  include <arpa/inet.h>
#endif

#include <memory>

#include <common/common_module.h>
#include <utils/common/concurrent.h>
#include <utils/common/log.h>
#include <utils/network/http/httpclient.h>
#include <utils/network/nettools.h>
#include <utils/network/ping.h>

#include "core/resource/resource_command_processor.h"
#include "core/resource/resource_command.h"
#include "core/resource_management/resource_data_pool.h"

#include "av_resource.h"
#include "av_panoramic.h"
#include "av_singesensor.h"


const QString QnPlAreconVisionResource::MANUFACTURE(lit("ArecontVision"));
#define MAX_RESPONSE_LEN (4*1024)


QnPlAreconVisionResource::QnPlAreconVisionResource()
    : m_totalMdZones(64),
      m_zoneSite(8),
      m_cameraReportedRTSPSupport(false)
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
    QVariant model;
    QVariant model_relase;

    if (!getParamPhysical(lit("model"), model))
        return QnNetworkResourcePtr(0);

    if (!getParamPhysical(lit("model=releasename"), model_relase))
        return QnNetworkResourcePtr(0);

    if (model_relase!=model)
    {
        //this camera supports release name
        model = model_relase;
    }
    else
    {
        //old camera; does not support relase name; but must support fullname
        if (getParamPhysical(lit("model=fullname"), model_relase))
            model = model_relase;
    }

    QnNetworkResourcePtr result(createResourceByName(model.toString()));
    if (result)
    {
        result->setName(model.toString());
        result->setHostAddress(getHostAddress());
        (result.dynamicCast<QnPlAreconVisionResource>())->setModel(model.toString());
        result->setMAC(getMAC());
        result->setId(getId());
        result->setFlags(flags());
    }
    else
    {
        NX_LOG( lit("Found unknown resource! %1").arg(model.toString()), cl_logWARNING);
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

        setParamPhysicalAsync(lit("sensorleft"), 0);
        setParamPhysicalAsync(lit("sensortop"), 0);
        setParamPhysicalAsync(lit("sensorwidth"), maxSensorWidth);
        setParamPhysicalAsync(lit("sensorheight"), maxSensorHeight);
    }

    QVariant firmwareVersion;
    if (!getParamPhysical(lit("fwversion"), firmwareVersion))
        return CameraDiagnostics::RequestFailedResult(lit("Firmware version"), lit("unknown"));

    QVariant val;
    if (!getParamPhysical(lit("procversion"), val))
        return CameraDiagnostics::RequestFailedResult(lit("Image engine"), lit("unknown"));

    if (!getParamPhysical(lit("netversion"), val))
        return CameraDiagnostics::RequestFailedResult(lit("Net version"), lit("unknown"));

    //if (!getDescription())
    //    return;

    setRegister(3, 21, 20); // sets I frame frequency to 1/20


    if (!setParamPhysical(lit("motiondetect"), lit("on"))) // enables motion detection;
        return CameraDiagnostics::RequestFailedResult(lit("Enable motion detection"), lit("unknown"));

    // check if we've got 1024 zones
    setParamPhysical(lit("mdtotalzones"), 1024); // try to set total zones to 64; new cams support it
    if (!getParamPhysical(lit("mdtotalzones"), val))
        return CameraDiagnostics::RequestFailedResult(lit("TotalZones"), lit("unknown"));

    if (val.toInt() == 1024)
        m_totalMdZones = 1024;

    // lets set zone size
    //one zone - 32x32 pixels; zone sizes are 1-15

    int optimal_zone_size_pixels = maxSensorWidth.toInt() / (m_totalMdZones == 64 ? 8 : 32);

    int zone_size = (optimal_zone_size_pixels%32) ? (optimal_zone_size_pixels/32 + 1) : optimal_zone_size_pixels/32;

    if (zone_size>15)
        zone_size = 15;

    if (zone_size<1)
        zone_size = 1;

    setFirmware(firmwareVersion.toString());
    saveParams();

    setParamPhysical(lit("mdzonesize"), zone_size);
    m_zoneSite = zone_size;
    setMotionMaskPhysical(0);

    //QVariant val;
    //if (getParamPhysical(lit("rtspSupported"), val))
    //    m_cameraReportedRTSPSupport = val.toBool();

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

QnMetaDataV1Ptr QnPlAreconVisionResource::getCameraMetadata()
{
    QnMetaDataV1Ptr motion(new QnMetaDataV1());
    QVariant mdresult;
    if (!getParamPhysical(QLatin1String("mdresult"), mdresult))
        return QnMetaDataV1Ptr(0);

    if (mdresult.toString() == QLatin1String("no motion"))
        return motion; // no motion detected


    int zones = totalMdZones() == 1024 ? 32 : 8;

    QStringList md = mdresult.toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
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


            if (m == QLatin1String("00") || m == QLatin1String("0"))
                continue;

            QRect currZoneRect = zeroZoneRect.translated(x*pixelZoneSize, y*pixelZoneSize);

            motion->mapMotion(imageRect, currZoneRect);

        }
    }

    //motion->m_duration = META_DATA_DURATION_MS * 1000 ;
    motion->m_duration = 1000 * 1000 * 1000; // 1000 sec 
    return motion;
}

// ===============================================================================================================================
bool QnPlAreconVisionResource::getParamPhysical(const QString &param, QVariant &val)
{
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());

    QString request = lit("get?") + param;

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

    val = QLatin1String(rarray.data());

    return true;
}

bool QnPlAreconVisionResource::setParamPhysical(const QString &param, const QVariant& val )
{
    QUrl devUrl(getUrl());
    CLSimpleHTTPClient connection(getHostAddress(), devUrl.port(80), getNetworkTimeout(), getAuth());

    QString request = lit("set?") + param;
    request += QLatin1Char('=') + val.toString();

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
            setParamPhysicalAsync(lit("mdlevelthreshold"), sensToLevelThreshold[sens]);
            break; // only 1 sensitivity for all frame is supported
        }
    }
}

bool QnPlAreconVisionResource::isRTSPSupported() const
{
    return m_cameraReportedRTSPSupport ||
           qnCommon->dataPool()->data(toSharedPointer(this)).
               value<bool>(lit("isRTSPSupported"), false);
}

bool QnPlAreconVisionResource::isAbstractResource() const
{
    QnUuid baseTypeId = qnResTypePool->getResourceTypeId(QnPlAreconVisionResource::MANUFACTURE, QLatin1String("ArecontVision_Abstract"));
    return getTypeId() == baseTypeId;
}

#endif
