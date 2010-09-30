#include "device_manager.h"
#include "..\asynch_seacher.h"
#include "..\src\corelib\thread\qmutex.h"
#include "..\directory_browser.h"
#include "settings.h"


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

	if (cr.mCriteria == CLDeviceCriteria::NONE)
	{
		return result; // empty
	}
	else if (cr.mCriteria == CLDeviceCriteria::ALL)
	{
		QMutexLocker lock(&m_dev_searcher.all_devices_mtx);
		CLDeviceList& devices =  m_dev_searcher.getAllDevices();
		foreach (CLDevice* device, devices)
		{
			device->addRef();
			result.insert(device->getUniqueId(),device);
		}

		return result;
	}



	// first implementation is very simple; returns all dev
	
}



void CLDeviceManager::onNewDevices_helper(CLDeviceList devices)
{
	
	foreach (CLDevice* device, devices)
	{
		if (device->getStatus().checkFlag(CLDeviceStatus::ALL) == 0  ) //&&  device->getMAC()=="00-1A-07-00-12-DB")
		{
			device->getStatus().setFlag(CLDeviceStatus::READY);

			QMutexLocker lock(&m_dev_searcher.all_devices_mtx);
			m_dev_searcher.getAllDevices().insert(device->getUniqueId(),device);

		}
		else
		{
			device->releaseRef();
		}
	}

}