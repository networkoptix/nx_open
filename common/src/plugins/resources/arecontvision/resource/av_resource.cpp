
#include "av_resource.h"
#include "av_panoramic.h"
#include "av_singesensor.h"
#include "core/resource/resource_command_consumer.h"

#include <QtNetwork/QUdpSocket>

#if defined(Q_OS_WIN)
#  include <winsock2.h>
#elif defined(QT_LINUXBASE)
#  include <arpa/inet.h>
#endif
#include "utils/network/nettools.h"
#include "utils/network/ping.h"

const char* QnPlAreconVisionResource::MANUFACTURE = "ArecontVision";

QnPlAreconVisionResource::QnPlAreconVisionResource()
    : m_totalMdZones(64)
{
    connect(this, SIGNAL(statusChanged(QnResource::Status,QnResource::Status)),
            this, SLOT(onStatusChanged(QnResource::Status,QnResource::Status)));
    setFlags(server_live_cam);
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


      void execute()
      {
          if (!isConnectedToTheResource())
              return;

          getResource().dynamicCast<QnPlAreconVisionResource>()->setRegister(m_page,m_reg,m_val);
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
        if (!ping.ping(ip.toString(), 2, ping_timeout)) // check if ip really changed
            return false;

    }

    return QnNetworkResource::setHostAddress(ip, domain);
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
    }
    else
    {
        cl_log.log("Found unknown resource! ", model.toString(), cl_logWARNING);
    }

    return result;
}

void QnPlAreconVisionResource::onStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus)
{
    if (!(oldStatus == Offline && newStatus == Online))
        return;

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

    setParamAsynch("sensorleft", 0, QnDomainPhysical);
    setParamAsynch("sensortop", 0, QnDomainPhysical);
    setParamAsynch("sensorwidth", maxSensorWidth, QnDomainPhysical);
    setParamAsynch("sensorheight", maxSensorHight, QnDomainPhysical);
}

int QnPlAreconVisionResource::totalMdZones() const
{
    return m_totalMdZones;
}

//===============================================================================================================================
bool QnPlAreconVisionResource::getParamPhysical(const QString& name, QVariant& val)
{
    //================================================
    QnParam param = getResourceParamList().value(name);
    if (param.netHelper().isEmpty()) // check if we have paramNetHelper
    {
        //cl_log.log("cannot find http command for such param!", cl_logWARNING);
        return false;
    }

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    QString request;

    QTextStream(&request) << "get?" << param.netHelper();

    if (connection.doGET(request)!=CL_HTTP_SUCCESS)
        return false;

    char c_response[200];

    int result_size =  connection.read(c_response,sizeof(c_response));

    if (result_size <0)
        return false;

    QByteArray response = QByteArray::fromRawData(c_response, result_size); // QByteArray  will not copy data

    int index = response.indexOf('=');
    if (index==-1)
        return false;

    QByteArray rarray = response.mid(index+1);

    QWriteLocker writeLocker(&m_rwLock);
    param.setValue(QString(rarray.data()));

    val = param.value();

    return true;
}

bool QnPlAreconVisionResource::setParamPhysical(const QString& name, const QVariant& val )
{
    QnParam param = getResourceParamList().value(name);

    if (param.netHelper().isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    QString request = QLatin1String("set?") + param.netHelper();
    if (param.type() != QnParamType::None && param.type() != QnParamType::Button)
        request += QLatin1Char('=') + val.toString();

    if (connection.doGET(request)!=CL_HTTP_SUCCESS)
        if (connection.doGET(request)!=CL_HTTP_SUCCESS) // try twice.
            return false;

    return true;
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

QnPlAreconVisionResource* QnPlAreconVisionResource::createResourceByTypeId(const QnId& rt)
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(rt);

    if (resourceType.isNull() || (resourceType->getManufacture() != MANUFACTURE))
    {
        cl_log.log("Can't create AV Resource. Resource type is invalid.", rt.toString(), cl_logERROR);
        return 0;
    }

    QnPlAreconVisionResource* res = 0;

    if (isPanoramic(resourceType->getName()))
        res = new CLArecontPanoramicResource(resourceType->getName());
    else
        res = new CLArecontSingleSensorResource(resourceType->getName());

    res->setTypeId(rt);

    return res;
}


bool QnPlAreconVisionResource::isPanoramic(const QString &name)
{
    return name.contains(QLatin1String("8180")) || name.contains(QLatin1String("8185")) ||
           name.contains(QLatin1String("8360")) || name.contains(QLatin1String("8365"));
}

QnAbstractStreamDataProvider* QnPlAreconVisionResource::createLiveDataProvider()
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "QnPlAreconVisionResource is abstract.");
    return 0;
}
