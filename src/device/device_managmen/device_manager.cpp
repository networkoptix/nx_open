#include "device_manager.h"
#include "..\asynch_seacher.h"
#include <QMutex>
#include "..\directory_browser.h"
#include "settings.h"
#include "recorder\fake_recorder_device.h"
#include <QDir>
#include <QFileInfoList>
#include "device_plugins\archive\archive\archive_device.h"

//=============================================================
CLDeviceManager& CLDeviceManager::instance()
{
	static CLDeviceManager inst ;
	return inst;
}

CLDeviceManager::CLDeviceManager():
m_firstTime(true)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
	m_timer.start(100); // first time should come fast

	addArchiver("Recorder:General Archiver");

	QString rootDir("c:/Photo/");

	QStringList subdirList = subDirList(rootDir);

	foreach(QString subdir, subdirList)
	{
		CLDirectoryBrowserDeviceServer dirbrowsr(rootDir + subdir);

		subdir = QString("Recorder:") + subdir;

		onNewDevices_helper(dirbrowsr.findDevices(), subdir);
		addArchiver(subdir);
	}

}

CLDeviceManager::~CLDeviceManager()
{
	m_timer.stop();
	m_dev_searcher.wait();

	{
		QMutexLocker lock(&m_dev_searcher.all_devices_mtx);
		CLDeviceList& list = m_dev_searcher.getAllDevices();
		CLDevice::deleteDevices(list);
	}

	foreach(CLDevice* dev, mRecDevices)
	{
		dev->releaseRef();
	}

	

}

CLDiviceSeracher& CLDeviceManager::getDiveceSercher()
{
	return m_dev_searcher;
}

void CLDeviceManager::onTimer()
{
	if (m_firstTime)
	{
		m_dev_searcher.start(); // first of all start fevice searcher
		m_firstTime = false;



		m_timer.setInterval(devices_update_interval);

	}


	if (!m_dev_searcher.isRunning() )
	{
		onNewDevices_helper(m_dev_searcher.result(), "Recorder:General Archiver");

		m_dev_searcher.start(); // run searcher again ...
	}
}



CLDeviceList CLDeviceManager::getDeviceList(CLDeviceCriteria& cr)
{
	CLDeviceList result;

	if (cr.getCriteria() == CLDeviceCriteria::NONE)
	{
		return result; // empty
	}
	else
	{
		QMutexLocker lock(&m_dev_searcher.all_devices_mtx);
		CLDeviceList& devices =  m_dev_searcher.getAllDevices();
		foreach (CLDevice* device, devices)
		{
			if (isDeviceMeetCriteria(cr, device))
			{
				device->addRef();
				result.insert(device->getUniqueId(),device);
			}
		}

		return result;
	}

	// first implementation is very simple; returns all dev
}

CLDevice* CLDeviceManager::getDeviceById(QString id)
{
	QMutexLocker lock(&m_dev_searcher.all_devices_mtx);
	CLDeviceList& devices =  m_dev_searcher.getAllDevices();
	if (devices.contains(id))
	{
		CLDevice* dev = devices[id];
		dev->addRef();

		return dev;
	}

	return 0;
}

CLDeviceList CLDeviceManager::getRecorderList()
{
	// gonna be different if we have server
	foreach(CLDevice* dev, mRecDevices)
	{
		dev->addRef();
	}

	return mRecDevices;

}

CLDevice* CLDeviceManager::getRecorderById(QString id)
{
	// gonna be different if we have server

	if (!mRecDevices.contains(id))
		return 0;

	CLDevice* dev = mRecDevices[id];
	dev->addRef();

	return dev;
}


CLDevice* CLDeviceManager::getArchiveDevice(QString id)
{
	QDir dir(id);
	if (!dir.exists())
		return 0;

	CLDevice* dev = new CLArchiveDevice(id);
	return dev;
}


void CLDeviceManager::onNewDevices_helper(CLDeviceList devices, QString parentId)
{
	
	foreach (CLDevice* device, devices)
	{
		if (device->getStatus().checkFlag(CLDeviceStatus::ALL) == 0  ) //&&  device->getMAC()=="00-1A-07-00-12-DB")
		{
			device->getStatus().setFlag(CLDeviceStatus::READY);

			device->setParentId(parentId);

			QMutexLocker lock(&m_dev_searcher.all_devices_mtx);
			m_dev_searcher.getAllDevices().insert(device->getUniqueId(),device);

		}
		else
		{
			device->releaseRef();
		}
	}

}


bool CLDeviceManager::isDeviceMeetCriteria(CLDeviceCriteria& cr, CLDevice* dev) const
{
	if (dev==0)
		return false;

	if (cr.getCriteria()== CLDeviceCriteria::NONE)
		return false;

	if (cr.getCriteria()== CLDeviceCriteria::ALL)
	{
		if (cr.getRecorderId()=="*")
			return true;

		if (dev->getParentId() == "")
			return false;



		if (dev->getParentId()!=cr.getRecorderId())
			return false;
	}

	if (cr.getCriteria()== CLDeviceCriteria::FILTER)
	{
		QStringList serach_list = cr.filter().split(" ", QString::SkipEmptyParts);
		QString dev_string = dev->toString();

		foreach(QString str, serach_list)
		{
			if (!dev_string.contains(str, Qt::CaseInsensitive))
				return false;
		}

	}


	return true;
	
}

QStringList CLDeviceManager::subDirList(const QString& abspath) const
{

	QStringList result;

	QDir dir(abspath);
	if (!dir.exists())
		return result;

	QFileInfoList list = dir.entryInfoList();

	foreach(QFileInfo info, list)
	{
		if (info.isDir() && info.fileName()!="." && info.fileName()!="..")
			result.push_back(info.fileName());
	}


	return result;

}

void CLDeviceManager::addArchiver(QString id)
{
	CLDevice* rec = new CLFakeRecorderDevice();
	rec->setUniqueId(id);
	mRecDevices[rec->getUniqueId()] = rec;
}