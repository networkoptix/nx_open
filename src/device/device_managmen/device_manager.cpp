#include "device_manager.h"
#include "..\asynch_seacher.h"
#include "..\src\corelib\thread\qmutex.h"
#include "..\directory_browser.h"
#include "settings.h"
#include "recorder\fake_recorder_device.h"

//=============================================================
CLDeviceCriteria::CLDeviceCriteria(CriteriaType cr):
mCriteria(cr)
{

}

CLDeviceCriteria::CriteriaType CLDeviceCriteria::getCriteria() const
{
	return mCriteria;
}

void CLDeviceCriteria::setRecorderId(QString id)
{
	mRecorderId = id;
}

QString CLDeviceCriteria::getRecorderId() const
{
	return mRecorderId;
}


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

	mRecDevice = new CLFakeRecorderDevice();
	mRecDevice->setUniqueId("Archiver 1");

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

	mRecDevice->releaseRef();

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

		CLDirectoryBrowserDeviceServer dirbrowsr("c:/Photo");
		onNewDevices_helper(dirbrowsr.findDevices());

		m_timer.setInterval(devices_update_interval);

	}


	if (!m_dev_searcher.isRunning() )
	{
		onNewDevices_helper(m_dev_searcher.result());

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

CLDeviceList CLDeviceManager::getRecorderList()
{
	// gonna be different if we have server
	CLDeviceList result;
	mRecDevice->addRef();
	result.insert(mRecDevice->getUniqueId(), mRecDevice);
	return result;
}

CLDevice* CLDeviceManager::getRecorderById(QString id)
{
	// gonna be different if we have server
	if (mRecDevice->getUniqueId()!=id)
		return 0;

	mRecDevice->addRef();

	return mRecDevice;
}


void CLDeviceManager::onNewDevices_helper(CLDeviceList devices)
{
	
	foreach (CLDevice* device, devices)
	{
		if (device->getStatus().checkFlag(CLDeviceStatus::ALL) == 0  ) //&&  device->getMAC()=="00-1A-07-00-12-DB")
		{
			device->getStatus().setFlag(CLDeviceStatus::READY);

			device->setParentId(mRecDevice->getUniqueId());

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


	return true;
	
}