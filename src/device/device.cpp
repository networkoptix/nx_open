#include "device.h"

#include "../base/log.h"
#include <QtConcurrentmap>
#include <QList>
#include <QTime>
#include "../network/nettools.h"
#include "../network/ping.h"
#include "../base/sleep.h"
#include "device_command_processor.h"
#include "device_video_layout.h"



CLDeviceCommandProcessor CLDevice::m_commanproc;

CLDevice::LL CLDevice::static_device_list; // list of all supported devices params list
CLDevice::LL CLDevice::static_stream_list; // list of all supported streams params list


CLDevice::CLDevice():
m_deviceTypeFlags(0),
m_videolayout(0)
{
	inst++;	
};


CLDevice::~CLDevice()
{
	delete m_videolayout;
	inst--;	
	cl_log.log("inst", inst, cl_logDEBUG1);
};

void CLDevice::setParentId(QString parentid)
{
	mParentId = parentid;
}

QString CLDevice::getParentId() const
{
	return mParentId;
}



QString CLDevice::getUniqueId() const
{
	return m_uniqueid;
}

void CLDevice::setUniqueId(const QString& id)
{
	m_uniqueid = id;
}


QString CLDevice::getName() const
{
	return m_name;
}

void CLDevice::setName(const QString& name)
{
	m_name = name;
}




CLDeviceStatus& CLDevice::getStatus()
{
	return m_status;
}

void  CLDevice::setStatus(const CLDeviceStatus& status)
{
	m_status = status;
}


CLParamList& CLDevice::getDevicePramList()
{
	if (m_deviceParamList.empty())
		m_deviceParamList = static_device_list[m_name];

	return m_deviceParamList;
}

const CLParamList& CLDevice::getDevicePramList() const
{
	if (m_deviceParamList.empty())
		m_deviceParamList = static_device_list[m_name];

	return m_deviceParamList;

}

bool CLDevice::checkDeviceTypeFlag(unsigned long flag) const
{
	return m_deviceTypeFlags & flag;
}

void CLDevice::addDeviceTypeFlag(unsigned long flag)
{
	m_deviceTypeFlags |= flag;
}

void CLDevice::removeDeviceTypeFlag(unsigned long flag)
{
	m_deviceTypeFlags &= ~flag;
}

CLParamList& CLDevice::getStreamPramList()
{
	if (m_streamParamList.empty())
		m_streamParamList = static_stream_list[m_name];

	return m_streamParamList;
}

const CLParamList& CLDevice::getStreamPramList() const
{
	if (m_streamParamList.empty())
		m_streamParamList = static_stream_list[m_name];

	return m_streamParamList;
}

QStringList CLDevice::supportedDevises()
{
	QStringList result;
	for (LL::iterator it = static_device_list.begin();it != static_device_list.end(); ++it)
		result.push_back(it.key());

	
	return result;
}

void CLDevice::setVideoLayout(CLDeviceVideoLayout* layout)
{
	delete m_videolayout;
	m_videolayout = layout;
}


const CLDeviceVideoLayout* CLDevice::getVideoLayout() const
{
	if (!m_videolayout)
		m_videolayout = new CLDefaultDeviceVideoLayout();

	return m_videolayout;

}

QString CLDevice::toString() const
{
	QString result;
	QTextStream(&result) << getName() << "  " <<  getUniqueId();
	return result;
}


void CLDevice::mergeLists(CLDeviceList& first, CLDeviceList second)
{
	CLDeviceList::iterator its = second.begin();
	while (its!=second.end())
	{
		CLDeviceList::iterator itf = first.find(its.key());

		if (itf==first.end())
		{
			first[its.key()] = its.value();
			second.erase(its++);
		}
		else
		{
			//delete its.value(); // here+
			its.value()->releaseRef();
			second.erase(its++);
		}
	}
}


void CLDevice::deleteDevices(CLDeviceList& lst)
{
	CLDeviceList::iterator it = lst.begin();
	while (it!=lst.end())
	{
		CLDevice* device = it.value();
		//delete device; //here+
		device->releaseRef(); 
		//lst.erase(it++);

		++it;
	}

}

void CLDevice::addReferences(CLDeviceList& lst)
{
	CLDeviceList::iterator it = lst.begin();
	while (it!=lst.end())
	{
		CLDevice* device = it.value();
		device->addRef();
		++it;
	}

}



void CLDevice::getDevicesBasicInfo(CLDeviceList& lst, int threads)
{
	// cannot make concurrent work with pointer CLDevice* ; => so extra steps needed
	struct T
	{
		T(CLDevice* d)
		{
			device = d;
		}

		void f()
		{
			device->getBaseInfo();
		}

		CLDevice* device;
	};


	cl_log.log("Geting device info...", cl_logDEBUG1);
	QTime time;
	time.start();

	QList<T> local_list;

	CLDeviceList::iterator it = lst.begin();
	while(it!=lst.end())
	{
		CLDevice* device = it.value();
		if (device->getStatus().checkFlag(CLDeviceStatus::CONFLICTING)==false)
			local_list.push_back(T(device));

		++it;
	}

	
	QThreadPool* global = QThreadPool::globalInstance();
	for (int i = 0; i < threads; ++i ) global->releaseThread();
	QtConcurrent::blockingMap(local_list, &T::f);
	for (int i = 0; i < threads; ++i )global->reserveThread();

	CL_LOG(cl_logDEBUG1)
	{
		cl_log.log("Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);

		for (CLDeviceList::iterator it = lst.begin(); it != lst.end(); ++it)
			cl_log.log(it.value()->toString(), cl_logDEBUG1);

	}

}


bool CLDevice::getParam(const QString& name, CLValue& val, bool resynch) 
{
	if (!getDevicePramList().exists(name))
	{
		cl_log.log("getParam: requested param does not exist!", cl_logWARNING);
		return false;
	}

	return true;
}


bool CLDevice::setParam(const QString& name, const CLValue& val)
{
	if (!getDevicePramList().exists(name))
	{
		cl_log.log("setParam: requested param does not exist!", cl_logWARNING);
		return false;
	}

	if (getDevicePramList().get(name).value.readonly)
	{
		cl_log.log("setParam: cannot set readonly param!", cl_logWARNING);
		return false;
	}

	return true;
	
}

bool CLDevice::setParam_asynch(const QString& name, const CLValue& val)
{

	class CLDeviceSetParamCommand : public CLDeviceCommand
	{
	public:
		CLDeviceSetParamCommand(CLDevice* dev, const QString& name, const CLValue& val):
		CLDeviceCommand(dev),
		m_name(name),
		m_val(val)
		{

		}

		void execute()
		{
			m_device->setParam(m_name, m_val);
		}
	private:
		QString m_name;
		CLValue m_val;

	};


	CLDeviceSetParamCommand *command = new CLDeviceSetParamCommand(this, name, val);
	m_commanproc.putData(command);
	command->releaseRef();
	
	return true;

}