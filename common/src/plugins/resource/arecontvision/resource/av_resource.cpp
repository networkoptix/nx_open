#ifdef ENABLE_ARECONT

#ifdef _WIN32
#  include <winsock2.h>
#endif

#if defined(QT_LINUXBASE)
#  include <arpa/inet.h>
#endif

#include <utils/common/log.h>
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
    : m_totalMdZones(64)
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

    CLSimpleHTTPClient http(getHostAddress(), 80,getNetworkTimeout(), getAuth());

    CLHttpStatus result = http.doGET(req);

    if (result!=CL_HTTP_SUCCESS)
        return result;

    char c_response[200];
    int result_size =  http.read(c_response,sizeof(c_response));

    if (result_size <0)
        return CL_TRANSPORT_ERROR;

    QByteArray arr = QByteArray::fromRawData(c_response, result_size); // QByteArray  will not copy data

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

    CLSimpleHTTPClient http(getHostAddress(), 80,getNetworkTimeout(), getAuth());

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

typedef QSharedPointer<QnPlArecontResourceSetRegCommand> QnPlArecontResourceSetRegCommandPtr;

CLHttpStatus QnPlAreconVisionResource::setRegister_asynch(int page, int num, int val)
{
    QnPlArecontResourceSetRegCommandPtr command ( new QnPlArecontResourceSetRegCommand(toSharedPointer(), page, num, val) );
    addCommandToProc(command);
    return CL_HTTP_SUCCESS;

}

bool QnPlAreconVisionResource::setHostAddress(const QString& hostAddr, QnDomain domain)
{
    // this will work only in local networks ( camera must be able ti get broadcat from this ip);
    QHostAddress ip = resolveAddress(hostAddr);
    if (domain == QnDomainPhysical)
    {
        return false; // never change ip 

        std::unique_ptr<AbstractDatagramSocket> sock( SocketFactory::createDatagramSocket() );

        sock->bind(getDiscoveryAddr().toString() , 0); // address usesd to find cam
        QString m_local_adssr_srt = getDiscoveryAddr().toString(); // debug only
        QString new_ip_srt = ip.toString(); // debug only

        QByteArray basic_str = "Arecont_Vision-AV2000\2";

        char data[256];

        int shift = 22;
        memcpy(data,basic_str.data(),shift);

        memcpy(data + shift, getMAC().bytes(), 6);//     MACsToByte(m_mac, (unsigned char*)data + shift); //memcpy(data + shift, mac, 6);

        quint32 new_ip = htonl(ip.toIPv4Address());
        memcpy(data + shift + 6, &new_ip,4);

        sock->sendTo(data, shift + 10, BROADCAST_ADDRESS, 69);
        sock->sendTo(data, shift + 10, BROADCAST_ADDRESS, 69);

        removeARPrecord(ip);
        removeARPrecord(resolveAddress(getHostAddress()));

        //
        CLPing ping;

        QString oldIp = getHostAddress();

        if (!ping.ping(ip.toString(), 2, ping_timeout)) // check if ip really changed
            return false;

    }

    return QnNetworkResource::setHostAddress(hostAddr, QnDomainMemory);
}

bool QnPlAreconVisionResource::unknownResource() const
{
    return (getName() == lit("ArecontVision_Abstract"));
}

QnResourcePtr QnPlAreconVisionResource::updateResource()
{
    QVariant model;
    QVariant model_relase;

    if (!getParam(lit("Model"), model, QnDomainPhysical))
        return QnNetworkResourcePtr(0);

    if (!getParam(lit("ModelRelease"), model_relase, QnDomainPhysical))
        return QnNetworkResourcePtr(0);

    if (model_relase!=model)
    {
        //this camera supports release name
        model = model_relase;
    }
    else
    {
        //old camera; does not support relase name; but must support fullname
        if (getParam(lit("ModelFull"), model_relase, QnDomainPhysical))
            model = model_relase;
    }

    QnNetworkResourcePtr result(createResourceByName(model.toString()));
    if (result)
    {
        result->setName(model.toString());
        result->setHostAddress(getHostAddress(), QnDomainMemory);
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

CameraDiagnostics::Result QnPlAreconVisionResource::initInternal()
{
    QnPhysicalCameraResource::initInternal();
    
    QVariant maxSensorWidth;
    QVariant maxSensorHeight;
    {
        // TODO: #Elric is this needed? This was a call to setCroppingPhysical
        getParam(lit("MaxSensorWidth"), maxSensorWidth, QnDomainMemory);
        getParam(lit("MaxSensorHeight"), maxSensorHeight, QnDomainMemory);

        setParamAsync(lit("sensorleft"), 0, QnDomainPhysical);
        setParamAsync(lit("sensortop"), 0, QnDomainPhysical);
        setParamAsync(lit("sensorwidth"), maxSensorWidth, QnDomainPhysical);
        setParamAsync(lit("sensorheight"), maxSensorHeight, QnDomainPhysical);
    }

    QVariant val;
    if (!getParam(lit("Firmware version"), val, QnDomainPhysical))
        return CameraDiagnostics::RequestFailedResult(lit("Firmware version"), lit("unknown"));

    if (!getParam(lit("Image engine"), val, QnDomainPhysical ))
        return CameraDiagnostics::RequestFailedResult(lit("Image engine"), lit("unknown"));

    if (!getParam(lit("Net version"), val, QnDomainPhysical))
        return CameraDiagnostics::RequestFailedResult(lit("Net version"), lit("unknown"));

    //if (!getDescription())
    //    return;

    setRegister(3, 21, 20); // sets I frame frequency to 1/20


    if (!setParam(lit("Enable motion detection"), lit("on"), QnDomainPhysical)) // enables motion detection;
        return CameraDiagnostics::RequestFailedResult(lit("Enable motion detection"), lit("unknown"));

    // check if we've got 1024 zones
    setParam(lit("TotalZones"), 1024, QnDomainPhysical); // try to set total zones to 64; new cams support it
    if (!getParam(lit("TotalZones"), val, QnDomainPhysical))
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

    //detecting and saving selected resolutions
    CameraMediaStreams mediaStreams;
    const CodecID streamCodec = isH264() ? CODEC_ID_H264 : CODEC_ID_MJPEG;
    mediaStreams.streams.push_back( CameraMediaStreamInfo( QSize(maxSensorWidth.toInt(), maxSensorHeight.toInt()), streamCodec ) );
    QVariant hasDualStreaming;
    getParam(lit("hasDualStreaming"), hasDualStreaming, QnDomainMemory);
    if( hasDualStreaming.toInt() > 0 )
        mediaStreams.streams.push_back( CameraMediaStreamInfo( QSize(maxSensorWidth.toInt()/2, maxSensorHeight.toInt()/2), streamCodec ) );
    saveResolutionList( mediaStreams );

    const QString firmware = getResourceParamList().value(lit("Firmware version")).value().toString();
    setFirmware(firmware);
    saveParams();

    setParam(lit("Zone size"), zone_size, QnDomainPhysical);
    setMotionMaskPhysical(0);

    return CameraDiagnostics::NoErrorResult();
}

QString QnPlAreconVisionResource::getDriverName() const
{
    return MANUFACTURE;
}

bool QnPlAreconVisionResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlAreconVisionResource::updateMACAddress()
{
    QVariant val;
    if (!getParam(lit("MACAddress"), val, QnDomainPhysical))
        return false;

    setMAC(QnMacAddress(val.toString()));

    return true;
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
    if (!hasParam(lit("Codec")))
        return false;

    QVariant val;
    getParam(lit("Codec"), val, QnDomainMemory);
    return val == lit("H.264");
}

//===============================================================================================================================
bool QnPlAreconVisionResource::getParamPhysical(const QnParam &param, QVariant &val)
{
    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    QString request = lit("get?") + param.netHelper();

    CLHttpStatus status = connection.doGET(request);
    if (status == CL_HTTP_AUTH_REQUIRED)
        setStatus(QnResource::Unauthorized);

    if (status != CL_HTTP_SUCCESS)
        return false;
        

    char c_response[MAX_RESPONSE_LEN];

    int result_size =  connection.read(c_response,sizeof(c_response));

    if (result_size <0)
        return false;

    QByteArray response = QByteArray::fromRawData(c_response, result_size); // QByteArray  will not copy data

    int index = response.indexOf('=');
    if (index==-1)
        return false;

    QByteArray rarray = response.mid(index+1);

    val = QLatin1String(rarray.data());

    return true;
}

bool QnPlAreconVisionResource::setParamPhysical(const QnParam &param, const QVariant& val )
{
    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    QString request = lit("set?") + param.netHelper();
    if (param.type() != Qn::PDT_None && param.type() != Qn::PDT_Button)
        request += QLatin1Char('=') + val.toString();

    CLHttpStatus status = connection.doGET(request);
    if (status != CL_HTTP_SUCCESS)
        status = connection.doGET(request);

    if (CL_HTTP_SUCCESS == status)
        return true;

    if (CL_HTTP_AUTH_REQUIRED == status)
    {
        setStatus(QnResource::Unauthorized);
        return false;
    }

    return false;
}

QnPlAreconVisionResource* QnPlAreconVisionResource::createResourceByName(const QString &name)
{
    QnId rt = qnResTypePool->getLikeResourceTypeId(MANUFACTURE, name);
    if (rt.isNull())
    {
        if ( name.left(2).toLower() == lit("av") )
        {
            QString new_name = name.mid(2);
            rt = qnResTypePool->getLikeResourceTypeId(MANUFACTURE, new_name);
            if (!rt.isNull())
            {
                NX_LOG( lit("Unsupported resource found(!!!): %1").arg(name), cl_logERROR);
                return 0;
            }
        }
    }

    return createResourceByTypeId(rt);

}

QnPlAreconVisionResource* QnPlAreconVisionResource::createResourceByTypeId(QnId rt)
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
	foreach(const QnParamTypePtr& param, resType->paramTypeList())
	{
		if (param->name == QLatin1String("VideoLayout"))
			return QnCustomResourceVideoLayout::fromString(param->default_value.toString())->channelCount() > 1;
	}
	return false;
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
        31, // 0 - aka mask really filtered by media server always
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

    QnMotionRegion region = m_motionMaskList[0];
    for (int sens = QnMotionRegion::MIN_SENSITIVITY+1; sens <= QnMotionRegion::MAX_SENSITIVITY; ++sens)
    {
        
        if (!region.getRegionBySens(sens).isEmpty())
        {
            setParamAsync(lit("Threshold"), sensToLevelThreshold[sens], QnDomainPhysical);
            break; // only 1 sensitivity for all frame is supported
        }
    }
}

#endif
