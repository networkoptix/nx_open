#include "device_manager.h"
#include "../asynch_seacher.h"
#include "settings.h"
#include "../network_device.h"
#include "../../device_plugins/archive/filetypesupport.h"
#include "../file_device.h"
#include "../../device_plugins/archive/avi_files/avi_dvd_device.h"
#include "../../device_plugins/archive/avi_files/avi_bluray_device.h"




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
m_firstTime(true),
mNeedresultsFromDirbrowsr(false)
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

	foreach(CLDevice* dev, mRecDevices)
	{
		dev->releaseRef();
	}

}

CLDeviceSearcher& CLDeviceManager::getDeviceSearcher()
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
		onNewDevices_helper(m_dev_searcher.result(), generalArchiverId);
		m_dev_searcher.start(); // run searcher again ...
	}

    if (mDirbrowsr.isRunning())
        return;

    if (mNeedresultsFromDirbrowsr)
    {
        //mDirbrowsr is not running 
        getResultFromDirBrowser();
        mNeedresultsFromDirbrowsr = false;
    }


    QStringList checklist = getPleaseCheckDirs();
    pleaseCheckDirs(QStringList()); // set an empty list;

    if (checklist.count())
    {
        mNeedresultsFromDirbrowsr = true;
        mDirbrowsr.setDirList(checklist);
        mDirbrowsr.start();
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


CLNetworkDevice* CLDeviceManager::getDeviceByIp(const QHostAddress& ip)
{
    QMutexLocker lock(&m_dev_searcher.all_devices_mtx);
    CLDeviceList& devices =  m_dev_searcher.getAllDevices();

    foreach(CLDevice* dev, devices)
    {
        if (!dev->checkDeviceTypeFlag(CLDevice::NETWORK))
            continue;

        CLNetworkDevice* nd = static_cast<CLNetworkDevice*>(dev);
        if (nd->getIP()== ip)
        {
            nd->addRef();
            return nd;
        }
    }

    return 0;
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


void CLDeviceManager::onNewDevices_helper(CLDeviceList devices, QString parentId)
{

    //int dev_per_arch  = devices.count()/10 + 1; // tests
    //int dev_count = 0;
              

	foreach (CLDevice* device, devices)
	{
		if (device->getStatus().checkFlag(CLDeviceStatus::ALL) == 0  ) //&&  device->getMAC()=="00-1A-07-00-12-DB")
		{
			device->getStatus().setFlag(CLDeviceStatus::READY);

            device->setParentId(parentId);
            
            //device->setParentId( QString::number(dev_count/dev_per_arch)); // tests
            //++dev_count;

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


void CLDeviceManager::getResultFromDirBrowser()
{
    CLDeviceList dev_list = mDirbrowsr.result();

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

void CLDeviceManager::addFiles(const QStringList& files)
{
    CLDeviceList lst;
    foreach(QString xfile, files)
    {

        //onNewDevices_helper does not check if devices already exist
        //so must be checked here

        CLDevice* dev = getDeviceById(xfile);

        if (dev)
        {
            dev->releaseRef();
            continue; // such dev already exists 
        }

        QFile file(xfile);
        if (!file.exists())
            continue;


        FileTypeSupport fileTypeSupport;

        if (fileTypeSupport.isImageFileExt(xfile))
        {
            dev = new CLFileDevice(xfile);
        }
        else if (CLAviDvdDevice::isAcceptedUrl(xfile))
        {
            dev = new CLAviDvdDevice(xfile);
        }
        else if (CLAviBluRayDevice::isAcceptedUrl(xfile))
        {
            dev = new CLAviBluRayDevice(xfile);
        }
        else if (fileTypeSupport.isMovieFileExt(xfile))
        {
            dev = new CLAviDevice(xfile);
        }
        else
        {
            continue;
        }

        lst[dev->getUniqueId()] = dev;
    }


    onNewDevices_helper(lst, generalArchiverId);
}
