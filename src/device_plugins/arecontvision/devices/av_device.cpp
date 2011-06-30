#include "av_device.h"
#include "../../../base/log.h"
#include "../../../base/sleep.h"
#include "../../../network/nettools.h"
#include "../../../network/ping.h"

#include "av_singesensor.h"
#include "av_panoramic.h"

#define CL_BROAD_CAST_RETRY 3

extern int ping_timeout;

CLHttpStatus CLAreconVisionDevice::getRegister(int page, int num, int& val)
{
	QString req;
	QTextStream(&req) << "getreg?page=" << page << "&reg=" << num;

	CLSimpleHTTPClient http(getIP(), 80,getHttpTimeout(), getAuth());
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

CLStreamreader* CLAreconVisionDevice::getDeviceStreamConnection()
{
	return 0;
}

bool CLAreconVisionDevice::unknownDevice() const
{
	return (getModel()==AVUNKNOWN);
}

CLNetworkDevice* CLAreconVisionDevice::updateDevice() 
{
	CLValue model;
	CLValue model_relase;

	if (!getParam("Model", model))
		return 0;

	if (!getParam("ModelRelease", model_relase))
		return 0;

	if (model_relase!=model)
	{
		//this camera supports release name
		model = model_relase;
	}
	else
	{
		//old camera; does not support relase name; but must support fullname
		if (getParam("ModelFull", model_relase))
			model = model_relase;
	}

	CLNetworkDevice* result = deviceByID(model, atoi( QString(model).toLatin1().data() )); // atoi here coz av 8185dn returns "8185DN" string on the get?model request; and QString::toInt returns 0 on such str

	if (!result)
	{
		cl_log.log("Found unknown device! ", QString(model), cl_logWARNING);
		return 0;
	}

	result->setName(model);
	result->setIP(getIP(), false);
	result->setMAC(getMAC());
	result->setUniqueId(getMAC());
	result->setLocalAddr(m_local_adssr);
	result->setStatus(getStatus());

	return result;
}

int CLAreconVisionDevice::getModel() const
{
	return m_model;
}

CLHttpStatus CLAreconVisionDevice::setRegister(int page, int num, int val)
{
	QString req;
	QTextStream(&req) << "setreg?page=" << page << "&reg=" << num << "&val=" << val;

	CLSimpleHTTPClient http(getIP(), 80,getHttpTimeout(), getAuth());
	http.setRequestLine(req);

	CLHttpStatus result = http.openStream();

	return result;

}

CLHttpStatus CLAreconVisionDevice::setRegister_asynch(int page, int num, int val)
{
	class CLDeviceSetRegCommand : public CLDeviceCommand
	{
	public:
		CLDeviceSetRegCommand(CLDevice* dev, int page, int reg, int val):
		  CLDeviceCommand(dev),
			  m_page(page),
			  m_val(val),
			  m_reg(reg)
		  {

		  }

		  void execute()
		  {
			  (static_cast<CLAreconVisionDevice*>(m_device))->setRegister(m_page,m_reg,m_val);
		  }
	private:
		int m_page;
		int m_reg;
		int m_val;
	};

	CLDeviceSetRegCommand *command = new CLDeviceSetRegCommand(this, page, num, val);
	m_commanproc.putData(command);
	command->releaseRef();

	return CL_HTTP_SUCCESS;

}

void CLAreconVisionDevice::onBeforeStart()
{
	CLValue maxSensorWidth;
	CLValue maxSensorHight;
	getParam("MaxSensorWidth", maxSensorWidth);
	getParam("MaxSensorHeight", maxSensorHight);

	setParam_asynch("sensorleft", 0);
	setParam_asynch("sensortop", 0);
	setParam_asynch("sensorwidth", maxSensorWidth);
	setParam_asynch("sensorheight", maxSensorHight);

}

bool CLAreconVisionDevice::getParam(const QString& name, CLValue& val, bool resynch )
{
	if (!CLDevice::getParam(name, val, resynch)) // check if param exists
		return false;

	CLParamType& value = getDeviceParamList().get(name).value;

	if (value.synchronized && !resynch) // if synchronized and do not need to do resynch
	{
		val = value.value;
		return true;
	}

	//================================================

	if (value.http=="") // check if we have http command for what
	{
		//cl_log.log("cannot find http command for such param!", cl_logWARNING);
		val = value.value;
		return true;

		//return false;
	}

	CLSimpleHTTPClient connection(getIP(), 80, getHttpTimeout(), getAuth());

	QString request;

	QTextStream(&request) << "get?" << value.http;

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

	value.value = QString(rarray.data());
	value.synchronized = true;

	val = value.value;

	return true;

}

bool CLAreconVisionDevice::setParam_special(const QString& name, const CLValue& val)
{
	return false;
}

bool CLAreconVisionDevice::setParam(const QString& name, const CLValue& val )
{

	if (!CLDevice::setParam(name, val))
		return false;

	if (setParam_special(name, val)) // try special first 
		return true;

	CLParamType& value = getDeviceParamList().get(name).value;

	//if (value.synchronized && value.value==val) // the same value
	//	return true;

	if (!value.setValue(val, false))
	{
		cl_log.log("cannot set such value!", cl_logWARNING);
		return false;
	}

	if (value.http=="") // check if we have http command for this param
	{
		value.setValue(val);
		return true;
	}

	CLSimpleHTTPClient connection(getIP(), 80, getHttpTimeout(), getAuth());

	QString request;

	QTextStream str(&request);
	str << "set?" << value.http;
	if (value.type!=CLParamType::None && value.type!=CLParamType::Button) 
		str << "=" << (QString)val;

	connection.setRequestLine(request);

	if (connection.openStream()!=CL_HTTP_SUCCESS)
		if (connection.openStream()!=CL_HTTP_SUCCESS) // try twice.
			return false;

	value.setValue(val);
	value.synchronized = true;

	return true;
}

bool CLAreconVisionDevice::executeCommand(CLDeviceCommand* command)
{
	return true;
}

CLDeviceList CLAreconVisionDevice::findDevices()
{
	CLDeviceList result;

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
				CLSleep::msleep(5);

		}

		// collecting response 
		QTime time;
		time.start();

		while(time.elapsed()<150)
		{
			//while (sock.hasPendingDatagrams()) 
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

				QString smac = MACToString(mac);
				if (result.find(smac)!=result.end())
					continue; // already found;

				QString id = "AVUNKNOWN";
				int model = 0;

				/*/
				int shift = 32;

				CLAreconVisionDevice* device = 0;

				if (datagram.size() > shift + 5)
				{
					model = (unsigned char)data[shift+2] * 256 + (unsigned char)data[shift+3]; //4
					QString smodel; 
					smodel.setNum(model);
					smodel = smodel;
					id = smodel; // this is not final version of the ID; it might/must be updated later 
					device = deviceByID(id, model);

					if (device)
						device->setName(id);
				}
				else
				{
					// very old cam; in future need to request model seporatly 
					device = new CLAreconVisionDevice(AVUNKNOWN);
					device->setName("AVUNKNOWN");

				}
				/**/

				// in any case let's HTTP do it's job at very end of discovery 
				CLAreconVisionDevice* device = new CLAreconVisionDevice(AVUNKNOWN);
				device->setName("AVUNKNOWN");

				if (device==0)
					continue;

				device->setIP(sender, false);
				device->setMAC(smac);
				device->setUniqueId(smac);

				CLDeviceStatus dh;
				device->m_local_adssr = ipaddrs.at(i);
				device->setStatus(dh);

				result[smac] = device;

			}

			CLSleep::msleep(2); // to avoid 100% cpu usage

		}

	}

	return result;
}

bool CLAreconVisionDevice::setIP(const QHostAddress& ip, bool net )
{
	// this will work only in local networks ( camera must be able ti get broadcat from this ip);

	if (net)
	{
		QUdpSocket sock;

		sock.bind(m_local_adssr , 0); // address usesd to find cam
		QString m_local_adssr_srt = m_local_adssr.toString(); // debug only
		QString new_ip_srt = ip.toString(); // debug only

		QByteArray basic_str = "Arecont_Vision-AV2000\2";

		char data[256];

		int shift = 22;
		memcpy(data,basic_str.data(),shift);
		MACsToByte(m_mac, (unsigned char*)data + shift); //memcpy(data + shift, mac, 6);

		quint32 new_ip = htonl(ip.toIPv4Address());
		memcpy(data + shift + 6, &new_ip,4);

		sock.writeDatagram(data, shift + 10,QHostAddress::Broadcast, 69);
		sock.writeDatagram(data, shift + 10,QHostAddress::Broadcast, 69);

		removeARPrecord(ip);
		removeARPrecord(getIP());

		//
		CLPing ping;
		if (!ping.ping(ip.toString(), 2, ping_timeout)) // check if ip really changed 
			return false; 

	}

	return CLNetworkDevice::setIP(ip);
}

bool CLAreconVisionDevice::loadDevicesParam(const QString& file_name, QString& error)
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
		if (node.toElement().tagName() == "device")
		{
			if (!parseDevice(node.toElement(), error))
				return false;
		}

		node = node.nextSibling();
	}

	return true;
}

bool CLAreconVisionDevice::parseDevice(const QDomElement &device, QString& error)
{
	if (!device.hasAttribute("id"))
	{
		error = "Cannot find id of the device.";
		return false;
	}

	QString device_name = device.attribute("id");

	LL::iterator it1 = static_device_list.find(device_name);
	LL::iterator it2 = static_stream_list.find(device_name);

	if (it1!=static_device_list.end() || it2!=static_stream_list.end())
	{
		QTextStream str(&error);
		str << "Such device " << device_name << " already exists.";
		return false;
	}

	CLParamList device_param_list;
	CLParamList stream_param_list;

	if (device.hasAttribute("public"))
	{
		QString values = device.attribute("public");
		QStringList lst = values.split(",");
		QStringListIterator lst_iter(lst);
		while(lst_iter.hasNext())
		{
			QString val = lst_iter.next().trimmed();
			LL::iterator it1 = static_device_list.find(val);
			LL::iterator it2 = static_stream_list.find(val);

			if (it1!=static_device_list.end())
				device_param_list.inheritedFrom(it1.value());
			else
				cl_log.log("Smth wrong with public tag for ", device_name, cl_logWARNING);

			if (it2!=static_stream_list.end())
				stream_param_list.inheritedFrom(it2.value());

		}

	}

	//global_params 
	QDomNode node = device.firstChild();
	if (node.isNull())
	{
		static_device_list[device_name] = device_param_list;
		static_stream_list[device_name] = stream_param_list;

		return true; // just public make sence 
	}

	QDomElement element = node.toElement();

	if (element.tagName()!="global_params" && element.tagName()!="stream_params")
	{
		QTextStream str(&error);
		str << "Cannot find global_params and stream_params in " << device_name ;
		return false;
	}

	//===============================================================
	if (element.tagName()=="global_params" )
	{
		QDomNode param_node = element.firstChild();
		while (!param_node .isNull()) 
		{
			CLParam param;

			if (param_node.toElement().tagName() == "param")
			{
				if (!parseParam(param_node.toElement(), error, device_param_list))
					return false;
			}
			param_node  = param_node.nextSibling();
		}

		static_device_list[device_name] = device_param_list;

		// move to stream_params
		node = node.nextSibling();
		if (node.isNull())
			return true;
		element = node.toElement();

	}
	//===================================================================
	if (element.tagName()=="stream_params")
	{
		QDomNode param_node = element.firstChild();
		while (!param_node .isNull()) 
		{
			CLParam param;

			if (param_node.toElement().tagName() == "param")
			{
				if (!parseParam(param_node.toElement(), error, stream_param_list))
					return false;
			}
			param_node  = param_node.nextSibling();
		}

		static_stream_list[device_name] = stream_param_list;
	}

	return true;
}

bool CLAreconVisionDevice::parseParam(const QDomElement &element, QString& error, CLParamList& paramlist)
{
	if (!element.hasAttribute("name"))
	{
		error = "Cannot find name attribute.";
		return false;
	}

	QString name = element.attribute("name");

	CLParam param = paramlist.exists(name) ? paramlist.get(name) : CLParam();
	param.name = name;

	if (param.value.type == CLParamType::None) // param type is not defined yet
	{
		if (!element.hasAttribute("type"))
		{
			error = "Cannot find type attribute.";
			return false;
		}

		QString type = element.attribute("type").toLower();

		if (type=="value")
			param.value.type = CLParamType::Value;
		else if (type=="novalue")
			param.value.type = CLParamType::None;
		else if (type=="minmaxstep")
			param.value.type = CLParamType::MinMaxStep;
		else if (type=="boolen")
			param.value.type = CLParamType::Boolen;
		else if (type=="onoff")
			param.value.type = CLParamType::OnOff;
		else if (type=="enumeration")
			param.value.type = CLParamType::Enumeration;
		else if (type=="button")
			param.value.type = CLParamType::Button;
		else
		{
			error = "Unsupported param type fund";
			return false;
		}
	}

	if (element.hasAttribute("group"))
		param.value.group = element.attribute("group");

	if (element.hasAttribute("sub_group"))
		param.value.subgroup = element.attribute("sub_group");

	if (element.hasAttribute("min"))
		param.value.min_val = element.attribute("min");

	if (element.hasAttribute("description"))
		param.value.description = element.attribute("description");

	if (element.hasAttribute("max"))
		param.value.max_val = element.attribute("max");

	if (element.hasAttribute("step"))
		param.value.step = element.attribute("step");

	if (element.hasAttribute("default_value"))
		param.value.default_value = element.attribute("default_value");

	if (element.hasAttribute("http"))
		param.value.http = element.attribute("http");

	if (element.hasAttribute("ui"))
		param.value.ui = element.attribute("ui").toInt();

	if (element.hasAttribute("readonly"))
	{
		if (element.attribute("readonly")=="true")
			param.value.readonly = true;
		else
			param.value.readonly = false;
	}

	if (element.hasAttribute("values"))
	{
		QString values = element.attribute("values");
		QStringList lst = values.split(",");
		QStringListIterator lst_iter(lst);
		while(lst_iter.hasNext())
		{
			QString val = lst_iter.next().trimmed();
			param.value.possible_values.push_back(val);
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
			param.value.ui_possible_values.push_back(val);
		}

	}

	if (element.hasAttribute("value"))
		param.value.value = element.attribute("value");
	else
		param.value.value = param.value.default_value;

	paramlist.put(param);

	return true;
}

CLAreconVisionDevice* CLAreconVisionDevice::deviceByID(QString id, int model)
{
	QStringList supp = CLDevice::supportedDevises();

	if (!supp.contains(id))
	{
		cl_log.log("Unsupported device found(!!!): ", id, cl_logERROR);
		return 0;
	}

	if (isPanoramic(model))
		return new  CLArecontPanoramicDevice(model);
	else
		return new CLArecontSingleSensorDevice(model);
}

bool CLAreconVisionDevice::getBaseInfo()
{
	CLValue val;
	if (!getParam("Firmware version", val))
		return false;

	if (!getParam("Image engine", val ))
		return false;

	if (!getParam("Net version", val))
		return false;

	if (!getDescription())
		return false;

	return true;
}

QString CLAreconVisionDevice::toString() const
{
	QString result;

	QString firmware = getDeviceParamList().get("Firmware version").value.value;
	QString hardware = getDeviceParamList().get("Image engine").value.value;
	QString net = getDeviceParamList().get("Net version").value.value;

	QTextStream t(&result);
	t<< CLDevice::toString() <<" live fw=" << firmware << " hw=" << hardware << " net=" << net;
	if (m_description!="")
		t <<  " dsc=" << m_description;
	return result;

}

bool CLAreconVisionDevice::isPanoramic(int model)
{
	return (model==AV8180 || model==AV8185 || model==AV8360 || model==AV8365);
}
