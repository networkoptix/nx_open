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

	if (!getParam(QLatin1String("Model"), model))
		return 0;

	if (!getParam(QLatin1String("ModelRelease"), model_relase))
		return 0;

	if (model_relase!=model)
	{
		//this camera supports release name
		model = model_relase;
	}
	else
	{
		//old camera; does not support relase name; but must support fullname
		if (getParam(QLatin1String("ModelFull"), model_relase))
			model = model_relase;
	}

	CLNetworkDevice* result = deviceByID(model, atoi( QString(model).toLatin1().data() )); // atoi here coz av 8185dn returns "8185DN" string on the get?model request; and QString::toInt returns 0 on such str

	if (!result)
	{
		cl_log.log(QLatin1String("Found unknown device! "), QString(model), cl_logWARNING);
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
        CLDeviceSetRegCommand(CLDevice* dev, int page, int reg, int val) :
            CLDeviceCommand(dev),
            m_page(page),
            m_reg(reg),
            m_val(val)
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
	getParam(QLatin1String("MaxSensorWidth"), maxSensorWidth);
	getParam(QLatin1String("MaxSensorHeight"), maxSensorHight);

	setParam_asynch(QLatin1String("sensorleft"), 0);
	setParam_asynch(QLatin1String("sensortop"), 0);
	setParam_asynch(QLatin1String("sensorwidth"), maxSensorWidth);
	setParam_asynch(QLatin1String("sensorheight"), maxSensorHight);

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

	if (value.http.isEmpty()) // check if we have http command for what
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

	value.value = rarray.data();
	value.synchronized = true;

	val = value.value;

	return true;

}

bool CLAreconVisionDevice::setParam_special(const QString& /*name*/, const CLValue& /*val*/)
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
		cl_log.log(QLatin1String("cannot set such value!"), cl_logWARNING);
		return false;
	}

	if (value.http.isEmpty()) // check if we have http command for this param
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

bool CLAreconVisionDevice::executeCommand(CLDeviceCommand* /*command*/)
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

				QString smac = MACToString(mac);
				if (result.find(smac)!=result.end())
					continue; // already found;

				QString id = QLatin1String("AVUNKNOWN");
//				int model = 0;

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
					device->setName(QLatin1String("AVUNKNOWN"));
				}
				*/

				// in any case let's HTTP do it's job at very end of discovery
				CLAreconVisionDevice* device = new CLAreconVisionDevice(AVUNKNOWN);
				device->setName(QLatin1String("AVUNKNOWN"));

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
		error = QLatin1String("Cannot open file ");
		error += file_name;
		return false;
	}

	QString errorStr;
	int errorLine;
	int errorColumn;
	QDomDocument doc;

	if (!doc.setContent(&file, &errorStr, &errorLine, &errorColumn))
	{
		QTextStream str(&error);
		str << QLatin1String("File ") << file_name << QLatin1String("; Parse error at line ") << errorLine << QLatin1String(" column ") << errorColumn << QLatin1String(" : ") << errorStr;
		return false;
	}

	QDomElement root = doc.documentElement();
	if (root.tagName() != QLatin1String("devices"))
		return false;

	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		if (node.toElement().tagName() == QLatin1String("device"))
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
	if (!device.hasAttribute(QLatin1String("id")))
	{
		error = QLatin1String("Cannot find id of the device.");
		return false;
	}

	QString device_name = device.attribute(QLatin1String("id"));

	LL::iterator it1 = static_device_list.find(device_name);
	LL::iterator it2 = static_stream_list.find(device_name);

	if (it1!=static_device_list.end() || it2!=static_stream_list.end())
	{
		QTextStream str(&error);
		str << QLatin1String("Such device ") << device_name << QLatin1String(" already exists.");
		return false;
	}

	CLParamList device_param_list;
	CLParamList stream_param_list;

	if (device.hasAttribute(QLatin1String("public")))
	{
		QString values = device.attribute(QLatin1String("public"));
		QStringList lst = values.split(QLatin1Char(','));
		QStringListIterator lst_iter(lst);
		while(lst_iter.hasNext())
		{
			QString val = lst_iter.next().trimmed();
			LL::iterator it1 = static_device_list.find(val);
			LL::iterator it2 = static_stream_list.find(val);

			if (it1!=static_device_list.end())
				device_param_list.inheritedFrom(it1.value());
			else
				cl_log.log(QLatin1String("Smth wrong with public tag for "), device_name, cl_logWARNING);

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

	if (element.tagName() != QLatin1String("global_params") && element.tagName() != QLatin1String("stream_params"))
	{
		QTextStream str(&error);
		str << QLatin1String("Cannot find global_params and stream_params in ") << device_name;
		return false;
	}

	//===============================================================
	if (element.tagName() == QLatin1String("global_params"))
	{
		QDomNode param_node = element.firstChild();
		while (!param_node .isNull())
		{
			CLParam param;
			if (param_node.toElement().tagName() == QLatin1String("param"))
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
	if (element.tagName() == QLatin1String("stream_params"))
	{
		QDomNode param_node = element.firstChild();
		while (!param_node .isNull())
		{
			CLParam param;
			if (param_node.toElement().tagName() == QLatin1String("param"))
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
	if (!element.hasAttribute(QLatin1String("name")))
	{
		error = QLatin1String("Cannot find name attribute.");
		return false;
	}

	QString name = element.attribute(QLatin1String("name"));

	CLParam param = paramlist.exists(name) ? paramlist.get(name) : CLParam();
	param.name = name;

	if (param.value.type == CLParamType::None) // param type is not defined yet
	{
		if (!element.hasAttribute(QLatin1String("type")))
		{
			error = QLatin1String("Cannot find type attribute.");
			return false;
		}

		QString type = element.attribute(QLatin1String("type")).toLower();

		if (type == QLatin1String("value"))
			param.value.type = CLParamType::Value;
		else if (type == QLatin1String("novalue"))
			param.value.type = CLParamType::None;
		else if (type == QLatin1String("minmaxstep"))
			param.value.type = CLParamType::MinMaxStep;
		else if (type == QLatin1String("boolen"))
			param.value.type = CLParamType::Boolen;
		else if (type == QLatin1String("onoff"))
			param.value.type = CLParamType::OnOff;
		else if (type == QLatin1String("enumeration"))
			param.value.type = CLParamType::Enumeration;
		else if (type == QLatin1String("button"))
			param.value.type = CLParamType::Button;
		else
		{
			error = QLatin1String("Unsupported param type fund");
			return false;
		}
	}

	if (element.hasAttribute(QLatin1String("group")))
		param.value.group = element.attribute(QLatin1String("group"));

	if (element.hasAttribute(QLatin1String("sub_group")))
		param.value.subgroup = element.attribute(QLatin1String("sub_group"));

	if (element.hasAttribute(QLatin1String("min")))
		param.value.min_val = element.attribute(QLatin1String("min"));

	if (element.hasAttribute(QLatin1String("description")))
		param.value.description = element.attribute(QLatin1String("description"));

	if (element.hasAttribute(QLatin1String("max")))
		param.value.max_val = element.attribute(QLatin1String("max"));

	if (element.hasAttribute(QLatin1String("step")))
		param.value.step = element.attribute(QLatin1String("step"));

	if (element.hasAttribute(QLatin1String("default_value")))
		param.value.default_value = element.attribute(QLatin1String("default_value"));

	if (element.hasAttribute(QLatin1String("http")))
		param.value.http = element.attribute(QLatin1String("http"));

	if (element.hasAttribute(QLatin1String("ui")))
		param.value.ui = element.attribute(QLatin1String("ui")).toInt();

	if (element.hasAttribute(QLatin1String("readonly")))
	{
		if (element.attribute(QLatin1String("readonly")) == QLatin1String("true"))
			param.value.readonly = true;
		else
			param.value.readonly = false;
	}

	if (element.hasAttribute(QLatin1String("values")))
	{
		QString values = element.attribute(QLatin1String("values"));
		QStringList lst = values.split(QLatin1Char(','));
		QStringListIterator lst_iter(lst);
		while(lst_iter.hasNext())
		{
			QString val = lst_iter.next().trimmed();
			param.value.possible_values.push_back(val);
		}

	}

	if (element.hasAttribute(QLatin1String("ui_values")))
	{
		QString values = element.attribute(QLatin1String("ui_values"));
		QStringList lst = values.split(QLatin1Char(','));
		QStringListIterator lst_iter(lst);
		while(lst_iter.hasNext())
		{
			QString val = lst_iter.next().trimmed();
			param.value.ui_possible_values.push_back(val);
		}
	}

	if (element.hasAttribute(QLatin1String("value")))
		param.value.value = element.attribute(QLatin1String("value"));
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
		cl_log.log(QLatin1String("Unsupported device found(!!!): "), id, cl_logERROR);
		return 0;
	}

	if (isPanoramic(model))
		return new CLArecontPanoramicDevice(model);
	return new CLArecontSingleSensorDevice(model);
}

bool CLAreconVisionDevice::getBaseInfo()
{
	CLValue val;
	if (!getParam(QLatin1String("Firmware version"), val))
		return false;

	if (!getParam(QLatin1String("Image engine"), val ))
		return false;

	if (!getParam(QLatin1String("Net version"), val))
		return false;

	if (!getDescription())
		return false;

	return true;
}

QString CLAreconVisionDevice::toString() const
{
	QString result;

	QString firmware = getDeviceParamList().get(QLatin1String("Firmware version")).value.value;
	QString hardware = getDeviceParamList().get(QLatin1String("Image engine")).value.value;
	QString net = getDeviceParamList().get(QLatin1String("Net version")).value.value;

	QTextStream t(&result);
	t << CLDevice::toString() << QLatin1String(" live fw=") << firmware << QLatin1String(" hw=") << hardware << QLatin1String(" net=") << net;
	if (!m_description.isEmpty())
		t << QLatin1String(" dsc=") << m_description;
	return result;

}

bool CLAreconVisionDevice::isPanoramic(int model)
{
	return (model==AV8180 || model==AV8185 || model==AV8360 || model==AV8365);
}
