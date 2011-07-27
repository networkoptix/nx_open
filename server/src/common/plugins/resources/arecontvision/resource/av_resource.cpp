#include "network/simple_http_client.h"
#include "av_resource.h"
#include "common/sleep.h"
#include "network/nettools.h"
#include "network/ping.h"
#include "av_panoramic.h"
#include "av_singesensor.h"
#include "resourcecontrol/resource_command_consumer.h"

#define CL_BROAD_CAST_RETRY 3

extern int ping_timeout;

QnPlAreconVisionResource::QnPlAreconVisionResource()
{
    
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
    QnValue maxSensorWidth;
    QnValue maxSensorHight;
    getParam("MaxSensorWidth", maxSensorWidth, QnDomainMemory);
    getParam("MaxSensorHeight", maxSensorHight, QnDomainMemory);

    setParamAsynch("sensorleft", 0, QnDomainPhysical);
    setParamAsynch("sensortop", 0, QnDomainPhysical);
    setParamAsynch("sensorwidth", maxSensorWidth, QnDomainPhysical);
    setParamAsynch("sensorheight", maxSensorHight, QnDomainPhysical);

}

QString QnPlAreconVisionResource::manufacture() const
{
    return "ArecontVision";
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


QnResourceList QnPlAreconVisionResource::findDevices()
{
	QnResourceList result;

	QList<QHostAddress> ipaddrs = getAllIPv4Addresses();

	CL_LOG(cl_logDEBUG1)
	{
		QString log;
		QTextStream(&log) << "CLAreconVisionDevice::findDevices  found " << ipaddrs.size() << " adapter(s) with IPV4";
		cl_log.log(log, cl_logDEBUG1);

		for (int i = 0; i < ipaddrs.size();++i)
		{
			QString slog;
			QTextStream(&slog) << ipaddrs.at(i).toString();
			cl_log.log(slog, cl_logDEBUG1);
		}
	}

	for (int i = 0; i < ipaddrs.size();++i)
	{
		QUdpSocket sock;
		sock.bind(ipaddrs.at(i), 0);

		// sending broadcast
		QByteArray datagram = "Arecont_Vision-AV2000\1";
		for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
		{
			sock.writeDatagram(datagram.data(), datagram.size(),QHostAddress::Broadcast, 69);

			if (r!=CL_BROAD_CAST_RETRY-1)
				QnSleep::msleep(5);

		}

		// collecting response 
		QTime time;
		time.start();

		while(time.elapsed()<150)
		{
			while (sock.hasPendingDatagrams()) 
			{
				QByteArray datagram;
				datagram.resize(sock.pendingDatagramSize());

				QHostAddress sender;
				quint16 senderPort;

				sock.readDatagram(datagram.data(), datagram.size(),	&sender, &senderPort);

				if (senderPort!=69 || datagram.size() < 32) // minimum response size
					continue;

				const unsigned char* data = (unsigned char*)(datagram.data());
				if (memcmp(data, "Arecont_Vision-AV2000", 21 )!=0)
					continue; // this responde id not from arecont camera

				unsigned char mac[6];
				memcpy(mac,data + 22,6);

                /*/
				QString smac = MACToString(mac);

				
                QString id = "AVUNKNOWN";
				int model = 0;

				
				int shift = 32;

				CLAreconVisionDevice* resource = 0;

				if (datagram.size() > shift + 5)
				{
					model = (unsigned char)data[shift+2] * 256 + (unsigned char)data[shift+3]; //4
					QString smodel; 
					smodel.setNum(model);
					smodel = smodel;
					id = smodel; // this is not final version of the ID; it might/must be updated later 
					resource = deviceByID(id, model);

					if (resource)
						resource->setName(id);
				}
				else
				{
					// very old cam; in future need to request model seporatly 
					resource = new CLAreconVisionDevice(AVUNKNOWN);
					resource->setName("AVUNKNOWN");

				}
				/**/

				// in any case let's HTTP do it's job at very end of discovery 
				QnNetworkResourcePtr resource ( new QnPlAreconVisionResource() );
				//resource->setName("AVUNKNOWN");

				if (resource==0)
					continue;

				resource->setHostAddress(sender, QnDomainPhysical);
				resource->setMAC(mac);
				resource->setDiscoveryAddr( ipaddrs.at(i) );

                if (hasEqualResource(result, resource))
                    continue; // already has such 

				result.push_back(resource);
			}

			QnSleep::msleep(2); // to avoid 100% cpu usage

		}

	}

	return result;
}


bool QnPlAreconVisionResource::loadDevicesParam(const QString& file_name, QString& error)
{
	QFile file(file_name);

	if (!file.exists())
	{
		error = "Cannot open file ";
		error += file_name;
		return false;
	}

	QString errorStr;
	int errorLine;
	int errorColumn;
	QDomDocument doc;

	if (!doc.setContent(&file, &errorStr, &errorLine,&errorColumn)) 
	{
		QTextStream str(&error);
		str << "File " << file_name << "; Parse error at line " << errorLine << " column " << errorColumn << " : " << errorStr;
		return false;
	}

	QDomElement root = doc.documentElement();
	if (root.tagName() != "devices")
		return false;

	QDomNode node = root.firstChild();

	while (!node.isNull()) 
	{
		if (node.toElement().tagName() == "resource")
		{
			if (!parseResource(node.toElement(), error))
				return false;
		}

		node = node.nextSibling();
	}

	return true;
}

bool QnPlAreconVisionResource::parseResource(const QDomElement &resource, QString& error)
{
	if (!resource.hasAttribute("id"))
	{
		error = "Cannot find id of the resource.";
		return false;
	}

	QString resourceName = resource.attribute("id");

	QnParamLists::iterator it1 = staticResourcesParamLists.find(resourceName);


	if (it1!=staticResourcesParamLists.end())
	{
		QTextStream str(&error);
		str << "Such resource " << resourceName << " already exists.";
		return false;
	}

	QnParamList device_param_list;

	if (resource.hasAttribute("public"))
	{
		QString values = resource.attribute("public");
		QStringList lst = values.split(",");
		QStringListIterator lst_iter(lst);
		while(lst_iter.hasNext())
		{
			QString val = lst_iter.next().trimmed();
			QnParamLists::iterator it1 = staticResourcesParamLists.find(val);

			if (it1!=staticResourcesParamLists.end())
				device_param_list.inheritedFrom(it1.value());
			else
				cl_log.log("Smth wrong with public tag for ", resourceName, cl_logWARNING);

		}

	}

	//global_params 
	QDomNode node = resource.firstChild();
	if (node.isNull())
	{
		staticResourcesParamLists[resourceName] = device_param_list;
		return true; // just public make sence 
	}

	QDomElement element = node.toElement();

	if (element.tagName()!="global_params" && element.tagName()!="stream_params")
	{
		QTextStream str(&error);
		str << "Cannot find global_params and stream_params in " << resourceName ;
		return false;
	}

	//===============================================================
	if (element.tagName()=="global_params" )
	{
		QDomNode paramNode = element.firstChild();
		while (!paramNode .isNull()) 
		{
			if (paramNode.toElement().tagName() == "param")
			{
				if (!parseParam(paramNode.toElement(), error, device_param_list))
					return false;
			}
			paramNode  = paramNode.nextSibling();
		}

		staticResourcesParamLists[resourceName] = device_param_list;

		// move to stream_params
		node = node.nextSibling();
		if (node.isNull())
			return true;
		element = node.toElement();

	}

	return true;
}

bool QnPlAreconVisionResource::parseParam(const QDomElement &element, QString& error, QnParamList& paramlist)
{
	if (!element.hasAttribute("name"))
	{
		error = "Cannot find name attribute.";
		return false;
	}

	QString name = element.attribute("name");

	QnParam param = paramlist.exists(name) ? paramlist.get(name) : QnParam();
	param.name = name;

	if (param.type == QnParam::None) // param type is not defined yet
	{
		if (!element.hasAttribute("type"))
		{
			error = "Cannot find type attribute.";
			return false;
		}

		QString type = element.attribute("type").toLower();

		if (type=="param")
			param.type = QnParam::Value;
		else if (type=="novalue")
			param.type = QnParam::None;
		else if (type=="minmaxstep")
			param.type = QnParam::MinMaxStep;
		else if (type=="boolen")
			param.type = QnParam::Boolen;
		else if (type=="onoff")
			param.type = QnParam::OnOff;
		else if (type=="enumeration")
			param.type = QnParam::Enumeration;
		else if (type=="button")
			param.type = QnParam::Button;
		else
		{
			error = "Unsupported param type fund";
			return false;
		}
	}

	if (element.hasAttribute("group"))
		param.group = element.attribute("group");

	if (element.hasAttribute("sub_group"))
		param.subgroup = element.attribute("sub_group");

	if (element.hasAttribute("min"))
		param.min_val = element.attribute("min");

	if (element.hasAttribute("description"))
		param.description = element.attribute("description");

	if (element.hasAttribute("max"))
		param.max_val = element.attribute("max");

	if (element.hasAttribute("step"))
		param.step = element.attribute("step");

	if (element.hasAttribute("default_value"))
		param.default_value = element.attribute("default_value");

	if (element.hasAttribute("paramNetHelper"))
		param.paramNetHelper = element.attribute("paramNetHelper");

	if (element.hasAttribute("ui"))
		param.ui = element.attribute("ui").toInt();

	if (element.hasAttribute("readonly"))
	{
		if (element.attribute("readonly")=="true")
			param.readonly = true;
		else
			param.readonly = false;
	}

	if (element.hasAttribute("values"))
	{
		QString values = element.attribute("values");
		QStringList lst = values.split(",");
		QStringListIterator lst_iter(lst);
		while(lst_iter.hasNext())
		{
			QString val = lst_iter.next().trimmed();
			param.possible_values.push_back(val);
		}

	}

	if (element.hasAttribute("ui_values"))
	{
		QString values = element.attribute("ui_values");
		QStringList lst = values.split(",");
		QStringListIterator lst_iter(lst);
		while(lst_iter.hasNext())
		{
			QString val = lst_iter.next().trimmed();
			param.ui_possible_values.push_back(val);
		}

	}

	if (element.hasAttribute("param"))
		param.value = element.attribute("param");
	else
		param.value = param.default_value;

	paramlist.put(param);

	return true;
}

QnPlAreconVisionResource* QnPlAreconVisionResource::createResourceByName(QString name)
{
	QStringList supp = QnResource::supportedDevises();

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
