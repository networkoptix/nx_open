#include "network/simple_http_client.h"
#include "av_resource.h"
#include "common/sleep.h"
#include "network/nettools.h"
#include "network/ping.h"
#include "av_panoramic.h"
#include "av_singesensor.h"
#include "resourcecontrol/resource_command_consumer.h"

const char* ArecontVisionManufacture = "ArecontVision";


#define CL_BROAD_CAST_RETRY 3
extern int ping_timeout;

QnPlAreconVisionResource::QnPlAreconVisionResource()
{
    
}

bool QnPlAreconVisionResource::isPanoramic() const
{
    return QnPlAreconVisionResource::isPanoramic(getName());
}

CLHttpStatus QnPlAreconVisionResource::getRegister(int page, int num, int& val)
{
	QString req;
	QTextStream(&req) << "getreg?page=" << page << "&reg=" << num;

	CLSimpleHTTPClient http(getHostAddress(), 80,getNetworkTimeout(), getAuth());
	http.setRequestLine(req);

	CLHttpStatus result = http.openStream();

	if (result!=CL_HTTP_SUCCESS)
		return result;

	char c_response[200];
	int result_size =  http.read(c_response,sizeof(c_response));

	if (result_size <0)
		return CL_HTTP_HOST_NOT_AVAILABLE;

	QByteArray arr = QByteArray::fromRawData(c_response, result_size); // QByteArray  will not copy data

	int index = arr.indexOf('=');
	if (index==-1)
		return CL_HTTP_HOST_NOT_AVAILABLE;

	QByteArray cnum = arr.mid(index+1);
	val = cnum.toInt();

	return CL_HTTP_SUCCESS;

}

CLHttpStatus QnPlAreconVisionResource::setRegister(int page, int num, int val)
{
    QString req;
    QTextStream(&req) << "setreg?page=" << page << "&reg=" << num << "&val=" << val;

    CLSimpleHTTPClient http(getHostAddress(), 80,getNetworkTimeout(), getAuth());
    http.setRequestLine(req);

    CLHttpStatus result = http.openStream();

    return result;

}

CLHttpStatus QnPlAreconVisionResource::setRegister_asynch(int page, int num, int val)
{
    class QnPlArecontResourceSetRegCommand : public QnResourceCommand
    {
    public:
        QnPlArecontResourceSetRegCommand(QnResourcePtr res, int page, int reg, int val):
          QnResourceCommand(res),
              m_page(page),
              m_val(val),
              m_reg(reg)
          {

          }

          void beforeDisconnectFromResource(){};

          void execute()
          {
              getResource().dynamicCast<QnPlAreconVisionResource>()->setRegister(m_page,m_reg,m_val);
          }
    private:
        int m_page;
        int m_reg;
        int m_val;
    };

    typedef QSharedPointer<QnPlArecontResourceSetRegCommand> QnPlArecontResourceSetRegCommandPtr;


    QnPlArecontResourceSetRegCommandPtr command ( new QnPlArecontResourceSetRegCommand(QnResourcePtr(this), page, num, val) );
    addCommandToProc(command);
    return CL_HTTP_SUCCESS;

}

bool QnPlAreconVisionResource::setHostAddress(const QHostAddress& ip, QnDomain domain)
{
    // this will work only in local networks ( camera must be able ti get broadcat from this ip);

    if (domain == QnDomainPhysical)
    {
        QUdpSocket sock;

        sock.bind(m_localAddress , 0); // address usesd to find cam
        QString m_local_adssr_srt = m_localAddress.toString(); // debug only
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

bool QnPlAreconVisionResource::getBasicInfo()
{
    QnValue val;
    if (!getParam("Firmware version", val, QnDomainPhysical))
        return false;

    if (!getParam("Image engine", val, QnDomainPhysical ))
        return false;

    if (!getParam("Net version", val, QnDomainPhysical))
        return false;

    if (!getDescription())
        return false;

    return true;
}

QString QnPlAreconVisionResource::toSearchString() const
{
    QString result;

    QString firmware = getResourceParamList().get("Firmware version").value;
    QString hardware = getResourceParamList().get("Image engine").value;
    QString net = getResourceParamList().get("Net version").value;

    QTextStream t(&result);
    t<< QnNetworkResource::toSearchString() <<" live fw=" << firmware << " hw=" << hardware << " net=" << net;

    if (m_description.isEmpty())
        t <<  " dsc=" << m_description;
    return result;

}


QnResourcePtr QnPlAreconVisionResource::updateResource() 
{
	QnValue model;
	QnValue model_relase;

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

	QnNetworkResourcePtr result ( createResourceByName(model) ); 

	if (!result)
	{
		cl_log.log("Found unknown resource! ", QString(model), cl_logWARNING);
		return QnNetworkResourcePtr(0);
	}

	result->setName(model);
	result->setHostAddress(getHostAddress(), QnDomainMemory);
	result->setMAC(getMAC());
	result->setId(getId());
	result->setNetworkStatus(m_networkStatus);


	return result;
}

void QnPlAreconVisionResource::beforeUse()
{
    QRect rect = getCroping(QnDomainMemory);
    setCropingPhysical(rect);
}

QString QnPlAreconVisionResource::manufacture() const
{
    return ArecontVisionManufacture;
}

bool QnPlAreconVisionResource::isResourceAccessible()
{
    return updateMACAddress();
}

bool QnPlAreconVisionResource::updateMACAddress()
{
    QnValue val;
    if (!getParam("MACAddress", val, QnDomainPhysical))
        return false;

    setMAC(QnMacAddress(val));

    return true;
}

QnStreamQuality QnPlAreconVisionResource::getBestQualityForSuchOnScreenSize(QSize size)
{
    return QnQualityNormal;
}

QnCompressedVideoDataPtr QnPlAreconVisionResource::getImage(int channnel, QDateTime time, QnStreamQuality quality)
{
    return QnCompressedVideoDataPtr(0);
}

int QnPlAreconVisionResource::getStreamDataProvidersMaxAmount() const
{
    return 0;
}

QnAbstractMediaStreamDataProvider* QnPlAreconVisionResource::createMediaProvider()
{
    return 0;
}

void QnPlAreconVisionResource::setIframeDistance(int frames, int timems)
{

}

void QnPlAreconVisionResource::setCropingPhysical(QRect croping)
{
    QnValue maxSensorWidth;
    QnValue maxSensorHight;
    getParam("MaxSensorWidth", maxSensorWidth, QnDomainMemory);
    getParam("MaxSensorHeight", maxSensorHight, QnDomainMemory);

    setParamAsynch("sensorleft", 0, QnDomainPhysical);
    setParamAsynch("sensortop", 0, QnDomainPhysical);
    setParamAsynch("sensorwidth", maxSensorWidth, QnDomainPhysical);
    setParamAsynch("sensorheight", maxSensorHight, QnDomainPhysical);
}


//===============================================================================================================================
bool QnPlAreconVisionResource::getParamPhysical(const QString& name, QnValue& val)
{
    QMutexLocker locker(&m_mutex);

    //================================================
    QnParam& param = getResourceParamList().get(name);
    if (param.paramNetHelper.isEmpty()) // check if we have paramNetHelper
    {
        //cl_log.log("cannot find http command for such param!", cl_logWARNING);
        return false;
    }

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    QString request;

    QTextStream(&request) << "get?" << param.paramNetHelper;

    connection.setRequestLine(request);

    if (connection.openStream()!=CL_HTTP_SUCCESS)
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

    param.value = QString(rarray.data());

    val = param.value;
    return true;

}

bool QnPlAreconVisionResource::setParamPhysical(const QString& name, const QnValue& val )
{
    QnParam& param = getResourceParamList().get(name);

    if (param.paramNetHelper.isEmpty()) // check if we have paramNetHelper command for this param
        return false;

    CLSimpleHTTPClient connection(getHostAddress(), 80, getNetworkTimeout(), getAuth());

    QString request;

    QTextStream str(&request);
    str << "set?" << param.paramNetHelper;
    if (param.type!=QnParam::None && param.type!=QnParam::Button) 
        str << "=" << (QString)val;

    connection.setRequestLine(request);

    if (connection.openStream()!=CL_HTTP_SUCCESS)
        if (connection.openStream()!=CL_HTTP_SUCCESS) // try twice.
            return false;


    return true;
}


QnPlAreconVisionResource* QnPlAreconVisionResource::createResourceByName(QString name)
{
	QStringList supp = QnResource::supportedResources(ArecontVisionManufacture);

	if (!supp.contains(name))
	{
		cl_log.log("Unsupported resource found(!!!): ", name, cl_logERROR);
		return 0;
	}

	if (isPanoramic(name))
		return new  CLArecontPanoramicDevice(name);
	else
		return new CLArecontSingleSensorDevice(name);
}



bool QnPlAreconVisionResource::isPanoramic(QString name)
{
	if (name.contains("8180"))
        return true;

    if (name.contains("8185"))
        return true;

    if (name.contains("8360"))
        return true;

    if (name.contains("8365"))
        return true;

    return false;
}
