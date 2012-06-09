
#include "av_resource.h"
#include "av_panoramic.h"
#include "av_singesensor.h"
#include "core/resource/resource_command_processor.h"

#include <QtNetwork/QUdpSocket>

#if defined(Q_OS_WIN)
#  include <winsock2.h>
#elif defined(QT_LINUXBASE)
#  include <arpa/inet.h>
#endif
#include "utils/network/nettools.h"
#include "utils/network/ping.h"

extern const char* ARECONT_VISION_MANUFACTURER;
const char* QnPlAreconVisionResource::MANUFACTURE = ARECONT_VISION_MANUFACTURER;
#define MAX_RESPONSE_LEN (4*1024)


QnPlAreconVisionResource::QnPlAreconVisionResource()
    : m_totalMdZones(64)
{
}

bool QnPlAreconVisionResource::isPanoramic() const
{
    return QnPlAreconVisionResource::isPanoramic(getName());
}

bool QnPlAreconVisionResource::isDualSensor() const
{
    const QString name = getName();
    return name.contains(QLatin1String("3130")) || name.contains(QLatin1String("3135"));
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
    QnPlArecontResourceSetRegCommand(QnResourcePtr res, int page, int reg, int val):
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

bool QnPlAreconVisionResource::setHostAddress(const QHostAddress& ip, QnDomain domain)
{
    // this will work only in local networks ( camera must be able ti get broadcat from this ip);

    if (domain == QnDomainPhysical)
    {
        return false; // never change ip 

        QUdpSocket sock;

        sock.bind(getDiscoveryAddr() , 0); // address usesd to find cam
        QString m_local_adssr_srt = getDiscoveryAddr().toString(); // debug only
        QString new_ip_srt = ip.toString(); // debug only

        QByteArray basic_str = "Arecont_Vision-AV2000\2";

        char data[256];

        int shift = 22;
        memcpy(data,basic_str.data(),shift);

        memcpy(data + shift, getMAC().toBytes(), 6);//     MACsToByte(m_mac, (unsigned char*)data + shift); //memcpy(data + shift, mac, 6);

        quint32 new_ip = htonl(ip.toIPv4Address());
        memcpy(data + shift + 6, &new_ip,4);

        sock.writeDatagram(data, shift + 10,QHostAddress::Broadcast, 69);
        sock.writeDatagram(data, shift + 10,QHostAddress::Broadcast, 69);

        removeARPrecord(ip);
        removeARPrecord(getHostAddress());

        //
        CLPing ping;

        QString oldIp = getHostAddress().toString();

        if (!ping.ping(ip.toString(), 2, ping_timeout)) // check if ip really changed
            return false;

    }

    return QnNetworkResource::setHostAddress(ip, QnDomainMemory);
}

QString QnPlAreconVisionResource::toSearchString() const
{
    QString result;

    const QnParamList params = getResourceParamList();
    const QString firmware = params.value("Firmware version").value().toString();
    const QString hardware = params.value("Image engine").value().toString();
    const QString net = params.value("Net version").value().toString();

    result += QnNetworkResource::toSearchString() + QLatin1String(" live fw=") + firmware + QLatin1String(" hw=") + hardware + QLatin1String(" net=") + net;
    if (!m_description.isEmpty())
        result += QLatin1String(" dsc=") + m_description;

    return result;
}

bool QnPlAreconVisionResource::unknownResource() const
{
    return (getName() == "ArecontVision_Abstract");
}

QnResourcePtr QnPlAreconVisionResource::updateResource()
{
    QVariant model;
    QVariant model_relase;

    if (!getParam("Model", model, QnDomainPhysical))
        return QnNetworkResourcePtr(0);

    if (!getParam("ModelRelease", model_relase, QnDomainPhysical))
        return QnNetworkResourcePtr(0);

    if (model_relase!=model)
    {
        //this camera supports release name
        model = model_relase;
    }
    else
    {
        //old camera; does not support relase name; but must support fullname
        if (getParam("ModelFull", model_relase, QnDomainPhysical))
            model = model_relase;
    }

    QnNetworkResourcePtr result(createResourceByName(model.toString()));
    if (result)
    {
        result->setName(model.toString());
        result->setHostAddress(getHostAddress(), QnDomainMemory);
        result->setMAC(getMAC());
        result->setId(getId());
        result->setFlags(flags());
    }
    else
    {
        cl_log.log("Found unknown resource! ", model.toString(), cl_logWARNING);
    }

    return result;
}

void QnPlAreconVisionResource::init()
{
    QRect rect = getCroping(QnDomainMemory);
    setCropingPhysical(rect);

    QVariant val;
    if (!getParam("Firmware version", val, QnDomainPhysical))
        return;

    if (!getParam("Image engine", val, QnDomainPhysical ))
        return;

    if (!getParam("Net version", val, QnDomainPhysical))
        return;

    //if (!getDescription())
    //    return;

    setRegister(3, 21, 20); // sets I frame frequency to 1/20


    if (!setParam("Enable motion detection", "on", QnDomainPhysical)) // enables motion detection;
        return;

    // check if we've got 1024 zones
    setParam("TotalZones", 1024, QnDomainPhysical); // try to set total zones to 64; new cams support it
    if (!getParam("TotalZones", val, QnDomainPhysical))
        return;

    if (val.toInt() == 1024)
        m_totalMdZones = 1024;

    // lets set zone size
    QVariant maxSensorWidth;
    getParam("MaxSensorWidth", maxSensorWidth, QnDomainMemory);

    //one zone - 32x32 pixels; zone sizes are 1-15

    int optimal_zone_size_pixels = maxSensorWidth.toInt() / (m_totalMdZones == 64 ? 8 : 32);

    int zone_size = (optimal_zone_size_pixels%32) ? (optimal_zone_size_pixels/32 + 1) : optimal_zone_size_pixels/32;

    if (zone_size>15)
        zone_size = 15;

    if (zone_size<1)
        zone_size = 1;

    setParam("Zone size", zone_size, QnDomainPhysical);
    setMotionMaskPhysical(0);
}

QString QnPlAreconVisionResource::manufacture() const
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
    if (!getParam("MACAddress", val, QnDomainPhysical))
        return false;

    setMAC(QnMacAddress(val.toString()));

    return true;
}

QnStreamQuality QnPlAreconVisionResource::getBestQualityForSuchOnScreenSize(QSize /*size*/) const
{
    return QnQualityNormal;
}

QImage QnPlAreconVisionResource::getImage(int /*channnel*/, QDateTime /*time*/, QnStreamQuality /*quality*/)
{
    return QImage();
}

void QnPlAreconVisionResource::setIframeDistance(int /*frames*/, int /*timems*/)
{
}

void QnPlAreconVisionResource::setCropingPhysical(QRect /*croping*/)
{
    QVariant maxSensorWidth;
    QVariant maxSensorHight;
    getParam("MaxSensorWidth", maxSensorWidth, QnDomainMemory);
    getParam("MaxSensorHeight", maxSensorHight, QnDomainMemory);

    setParamAsync("sensorleft", 0, QnDomainPhysical);
    setParamAsync("sensortop", 0, QnDomainPhysical);
    setParamAsync("sensorwidth", maxSensorWidth, QnDomainPhysical);
    setParamAsync("sensorheight", maxSensorHight, QnDomainPhysical);
}

int QnPlAreconVisionResource::totalMdZones() const
{
    return m_totalMdZones;
}

//===============================================================================================================================
bool QnPlAreconVisionResource::getParamPhysical(const QnParam &param, QVariant &val)
{
    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    QString request = QLatin1String("get?") + param.netHelper();

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

    val = QString(rarray.data());

    return true;
}

bool QnPlAreconVisionResource::setParamPhysical(const QnParam &param, const QVariant& val )
{
    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    QString request = QLatin1String("set?") + param.netHelper();
    if (param.type() != QnParamType::None && param.type() != QnParamType::Button)
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
    QnId rt = qnResTypePool->getResourceTypeId(MANUFACTURE, name);
    if (!rt.isValid())
    {
        cl_log.log("Unsupported resource found(!!!): ", name, cl_logERROR);
        return 0;
    }

    return createResourceByTypeId(rt);
}

QnPlAreconVisionResource* QnPlAreconVisionResource::createResourceByTypeId(QnId rt)
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(rt);

    if (resourceType.isNull() || (resourceType->getManufacture() != MANUFACTURE))
    {
        cl_log.log("Can't create AV Resource. Resource type is invalid.", rt.toString(), cl_logERROR);
        return 0;
    }

    QnPlAreconVisionResource* res = 0;

    if (isPanoramic(resourceType->getName()))
        res = new QnArecontPanoramicResource(resourceType->getName());
    else
        res = new CLArecontSingleSensorResource(resourceType->getName());

    res->setTypeId(rt);

    return res;
}

bool QnPlAreconVisionResource::isPanoramic(const QString &name)
{
    return name.contains(QLatin1String("8180")) || name.contains(QLatin1String("8185")) || name.contains(QLatin1String("20185")) ||
           name.contains(QLatin1String("8360")) || name.contains(QLatin1String("8365"));
}

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
            setParamAsync("Threshold", sensToLevelThreshold[sens], QnDomainPhysical);
            break; // only 1 sensitivity for all frame is supported
        }
    }
}

