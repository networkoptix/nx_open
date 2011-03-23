#include "device_manager.h"
#include "../asynch_seacher.h"
#include "../directory_browser.h"
#include "settings.h"
#include "recorder/fake_recorder_device.h"
#include "device_plugins/archive/archive/archive_device.h"
#include "util.h"

// Init static variables
CLDeviceManager* CLDeviceManager::m_Instance = 0;



QString generalArchiverId = "Recorder:General Archiver";

//=============================================================
CLDeviceManager& CLDeviceManager::instance()
{
	if (m_Instance == 0) {
		m_Instance = new CLDeviceManager();
	}

	return *m_Instance;
}

CLDeviceManager::CLDeviceManager():
m_firstTime(true)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));
	m_timer.start(100); // first time should come fast

	addArchiver(generalArchiverId);

    QStringList checkLst;
    checkLst.push_back(getMediaRootDir());
    pleaseCheckDirs(checkLst);

	//CLDirectoryBrowserDeviceServer dirbrowsr(getMediaRootDir());
	//onNewDevices_helper(dirbrowsr.findDevices(), generalArchiverId);
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

void CLDeviceManager::pleaseCheckDirs(const QStringList& lst)
{
    QMutexLocker mutex(&mPleaseCheckDirsLst_cs);
    mPleaseCheckDirsLst = lst;
}

QStringList CLDeviceManager::getPleaseCheckDirs() const
{
    QMutexLocker mutex(&mPleaseCheckDirsLst_cs);
    return mPleaseCheckDirsLst;
}

void CLDeviceManager::onTimer()
{
	if (m_firstTime)
	{
		m_dev_searcher.start(); // first of all start device searcher
		m_firstTime = false;

		m_timer.setInterval(devices_update_interval);

	}

	if (!m_dev_searcher.isRunning() )
	{
		onNewDevices_helper(m_dev_searcher.result(), "Recorder:General Archiver");
		m_dev_searcher.start(); // run searcher again ...
	}


    QStringList checklist = getPleaseCheckDirs();
    pleaseCheckDirs(QStringList()); // set an empty list;

    if (checklist.count())
    {
        QTime time; time.restart();
        cl_log.log("=====about to check dirs...", cl_logALWAYS);

        // if the have something to check

        foreach(QString dir, checklist)
        {
            CLDirectoryBrowserDeviceServer dirbrowsr(dir);
            CLDeviceList dev_list = dirbrowsr.findDevices();

            {
                // exclude already existing devices 
                QMutexLocker lock(&m_dev_searcher.all_devices_mtx);
                CLDeviceList& all_devices =  m_dev_searcher.getAllDevices();

                CLDeviceList::iterator it = dev_list.begin(); 
                while (it!=dev_list.end())
                {
                    if (!all_devices.contains(it.key()))
                    {
                        ++it;
                        continue;
                    }

                    it.value()->releaseRef();
                    dev_list.erase(it++);
                }

            }

            onNewDevices_helper(dev_list, generalArchiverId);

        }

        

        cl_log.log("=====done checking dirs. Time elapsed = ", time.elapsed(), cl_logALWAYS);


    }

}

CLDeviceList CLDeviceManager::getDeviceList(const CLDeviceCriteria& cr)
{
	CLDeviceList result;

	if (cr.getCriteria() == CLDeviceCriteria::STATIC || cr.getCriteria() == CLDeviceCriteria::NONE )
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

bool CLDeviceManager::isDeviceMeetCriteria(const CLDeviceCriteria& cr, CLDevice* dev) const
{
	if (dev==0)
		return false;

	if (cr.getCriteria()== CLDeviceCriteria::STATIC)
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
        if (cr.filter().length()==0)
            return false;


        QString dev_string = dev->toString();

        bool matches = false;


        QStringList serach_list = cr.filter().split('+', QString::SkipEmptyParts);
        foreach(QString sub_filter, serach_list)
        {
            if (serach_list.count()<2 || sub_filter.length()>2)
                matches = matches || match_subfilter(dev_string, sub_filter);
        }


        return matches;

	}

	return true;

}


void CLDeviceManager::addArchiver(QString id)
{
	CLDevice* rec = new CLFakeRecorderDevice();
	rec->setUniqueId(id);
	mRecDevices[rec->getUniqueId()] = rec;
}


//=========================================

bool CLDeviceManager::match_subfilter(QString dev, QString fltr) const
{
    QStringList serach_list = fltr.split(" ", QString::SkipEmptyParts);

    foreach(QString str, serach_list)
    {
        if (!dev.contains(str, Qt::CaseInsensitive))
            return false;
    }

    return (serach_list.count());

}
