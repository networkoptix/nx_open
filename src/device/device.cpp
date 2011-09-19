#include "device.h"

#include "../base/log.h"
#include "../network/nettools.h"
#include "../network/ping.h"
#include "../base/sleep.h"
#include "device_command_processor.h"
#include "device_video_layout.h"

static int inst = 0;

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
	cl_log.log(QLatin1String("inst"), inst, cl_logDEBUG1);
};

void CLDevice::setParentId(QString parentid)
{
	m_parentId = parentid;
}

QString CLDevice::getParentId() const
{
	return m_parentId;
}

QString CLDevice::getUniqueId() const
{
	return m_uniqueId;
}

void CLDevice::setUniqueId(const QString& id)
{
	m_uniqueId = id;
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

CLParamList& CLDevice::getDeviceParamList()
{
	if (m_deviceParamList.empty())
		m_deviceParamList = static_device_list[m_name];

	return m_deviceParamList;
}

const CLParamList& CLDevice::getDeviceParamList() const
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

CLParamList& CLDevice::getStreamParamList()
{
	if (m_streamParamList.empty())
		m_streamParamList = static_stream_list[m_name];

	return m_streamParamList;
}

const CLParamList& CLDevice::getStreamParamList() const
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

const CLDeviceVideoLayout* CLDevice::getVideoLayout(CLStreamreader* reader)
{
	if (!m_videolayout)
		m_videolayout = new CLDefaultDeviceVideoLayout();

	return m_videolayout;

}

bool CLDevice::associatedWithFile() const
{
    return checkDeviceTypeFlag(CLDevice::ARCHIVE) || checkDeviceTypeFlag(CLDevice::RECORDED) || checkDeviceTypeFlag(CLDevice::SINGLE_SHOT);
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

namespace
{
    bool NotConflicting(CLDevice* device)
    {
        return device->getStatus().checkFlag(CLDeviceStatus::CONFLICTING) == false;
    }
}

void CLDevice::getDevicesBasicInfo(CLDeviceList& lst, int threads)
{
	// cannot make concurrent work with pointer CLDevice* ; => so extra steps needed

	cl_log.log(QLatin1String("Geting device info..."), cl_logDEBUG1);
	QTime time;
	time.start();

        QList<CLDevice*> local_list;

        std::remove_copy_if(lst.begin(), lst.end(), std::back_inserter(local_list), std::ptr_fun(NotConflicting));

	QThreadPool* global = QThreadPool::globalInstance();
	for (int i = 0; i < threads; ++i ) global->releaseThread();
        QtConcurrent::blockingMap(local_list, std::mem_fun(&CLDevice::getBaseInfo));
	for (int i = 0; i < threads; ++i )global->reserveThread();

	CL_LOG(cl_logDEBUG1)
	{
		cl_log.log(QLatin1String("Done. Time elapsed: "), time.elapsed(), cl_logDEBUG1);

		for (CLDeviceList::iterator it = lst.begin(); it != lst.end(); ++it)
			cl_log.log(it.value()->toString(), cl_logDEBUG1);

	}

}

bool CLDevice::getParam(const QString& name, CLValue& /*val*/, bool /*resynch*/)
{
	if (!getDeviceParamList().exists(name))
	{
		cl_log.log(QLatin1String("getParam: requested param does not exist!"), cl_logWARNING);
		return false;
	}

	return true;
}

bool CLDevice::setParam(const QString& name, const CLValue& /*val*/)
{
	if (!getDeviceParamList().exists(name))
	{
		cl_log.log(QLatin1String("setParam: requested param does not exist!"), cl_logWARNING);
		return false;
	}

	if (getDeviceParamList().get(name).value.readonly)
	{
		cl_log.log(QLatin1String("setParam: cannot set readonly param!"), cl_logWARNING);
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
