#include "asynch_seacher.h"
#include "../base/log.h"
#include "deviceserver.h"
#include "../network/nettools.h"
#include "network_device.h"
#include "settings.h"

CLDeviceSearcher::CLDeviceSearcher()
{

}

CLDeviceSearcher::~CLDeviceSearcher()
{
	wait();
}

void CLDeviceSearcher::addDeviceServer(CLDeviceServer* serv)
{
	QMutexLocker lock(&m_servers_mtx);
	m_servers.push_back(serv);
}

CLDeviceList CLDeviceSearcher::result()
{
	return m_result;
}

void CLDeviceSearcher::run()
{
	bool ip_finished;
	m_result = findNewDevices(ip_finished);

	if (ip_finished)
	{
		static bool first_time = true;
		const QString& message = QObject::trUtf8("Cannot get available IP address.");
		CL_LOG(cl_logWARNING) cl_log.log(message ,cl_logWARNING);
		if (first_time)
		{
			QMessageBox* box = new QMessageBox(QMessageBox::Warning, "Info", message, QMessageBox::Ok, 0);
			box->show();
			first_time = false;
		}
	}

}

CLDeviceList CLDeviceSearcher::findNewDevices(bool& ip_finished)
{

    bool allow_to_change_ip = Settings::instance().isAllowChangeIP();

	ip_finished = false;

	CLDeviceList::iterator it;
	QTime time;
	time.start();

	m_netState.updateNetState(); // update net state before serach 

	//====================================
	CL_LOG(cl_logDEBUG1) cl_log.log("looking for devices...", cl_logDEBUG1);

	CLDeviceList devices;

	{
		QMutexLocker lock(&m_servers_mtx);
		for (int i = 0; i < m_servers.size(); ++i)
		{
			CLDeviceServer* serv = m_servers.at(i);
			CLDeviceList temp = serv->findDevices();
			CLDevice::mergeLists(devices, temp);
		}

	}
	//

	//excluding already existing devices with READY status
	{
		QMutexLocker lock(&all_devices_mtx);

		it = devices.begin(); 
		while (it!=devices.end())
		{
			const QString& unique = it.key();
			CLDeviceList::iterator all_it = all_devices.find(unique);

			if (all_it == all_devices.end())
			{
				++it;
				continue;
			}

			CLDevice* all_dev = all_it.value();
			if (all_dev->getStatus().checkFlag(CLDeviceStatus::READY)) // such device already exists
			{
				// however, ip address or mask may be changed( very unlikely but who knows )
				// and this is why we need to check if old( already existing device) still is in our subnet 
				if (all_dev->checkDeviceTypeFlag(CLDevice::NETWORK))
				{
					CLNetworkDevice* all_dev_net = static_cast<CLNetworkDevice*>(all_dev);
					if (!m_netState.isInMachineSubnet(all_dev_net->getIP())) // not the same network
					{
						//[-1-]
						// we do changed ip address or subnet musk or even removed old nic interface and likely this device has "lost connection status"
						// ip address must be changed

						CLNetworkDevice* new_dev_net = static_cast<CLNetworkDevice*>(it.value());
						all_dev_net->setDiscoveryAddr(new_dev_net->getDiscoveryAddr()); // update DiscoveryAddr; it will help to find new available ip in subnet
						all_dev_net->getStatus().setFlag(CLDeviceStatus::NOT_IN_SUBNET);
					}

				}

				//delete it.value(); //here+
				it.value()->releaseRef();
				devices.erase(it++);
			}
			else
				++it; // new device; must stay

		}

	}
	//====================================
	// at this point in devices we have all new found devices
	CLDeviceList not_network_devices;

	// remove all not network devices from list 
	{
		CLDeviceList::iterator it = devices.begin();
		while (it!=devices.end())
		{
			CLDevice* device = it.value();

			if (!device->checkDeviceTypeFlag(CLDevice::NETWORK)) // is it network device
			{
				not_network_devices[device->getUniqueId()] = device;
				devices.erase(it++);
			}
			else
				++it;
		}
	}

	// now devices list has only network devices 

	// lets form the list of existing IP
	CLIPList busy_list;

	CLDeviceList bad_ip_list;

	cl_log.log("Found ", devices.size() + not_network_devices.size(), " new(!) devices.", cl_logDEBUG1);

	CL_LOG(cl_logDEBUG2)
	{
		for (CLDeviceList::iterator it = devices.begin(); it != devices.end(); ++it)
			cl_log.log(it.value()->toString(), cl_logDEBUG2);
	}

	cl_log.log("Time elapsed: ", time.elapsed(), cl_logDEBUG1);

	if (devices.size()==0) // no new devices
		goto END;

	//====================================
	checkObviousConflicts(devices); // if conflicting it will mark it
	//====================================

	// check if all in our subnet

	foreach(CLDevice* dev, devices)
	{
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(dev);

		if ((!device->isAfterRouter()) && (!m_netState.isInMachineSubnet(device->getIP()))) // not the same network
			device->getStatus().setFlag(CLDeviceStatus::NOT_IN_SUBNET);
	}

	//=====================================

	// move all conflicting cams and cams with bad ip to bad_ip_list
	fromListToList(devices, bad_ip_list, CLDeviceStatus::NOT_IN_SUBNET, CLDeviceStatus::NOT_IN_SUBNET);
	fromListToList(devices, bad_ip_list, CLDeviceStatus::CONFLICTING, CLDeviceStatus::CONFLICTING);

	time.restart();

	cl_log.log("Checking for real conflicts ", devices.size(), " devices.", cl_logDEBUG1);
	markConflictingDevices(devices,5);
	fromListToList(devices, bad_ip_list, CLDeviceStatus::CONFLICTING, CLDeviceStatus::CONFLICTING);
	cl_log.log("Time elapsed ", time.restart(), cl_logDEBUG1);
	cl_log.log(" ", devices.size(), " new(!) devices not conflicting .", cl_logDEBUG1);

	//======================================

	// now in devices only new non conflicting devices; in bad_ip_list only devices with conflicts, so ip of bad_ip_list must be chnged 
	if (!allow_to_change_ip)// nothing elese we can do
	{
		// move all back to devices
		fromListToList(bad_ip_list, devices,  CLDeviceStatus::CONFLICTING, CLDeviceStatus::CONFLICTING);
		fromListToList(bad_ip_list, devices, CLDeviceStatus::NOT_IN_SUBNET, CLDeviceStatus::NOT_IN_SUBNET);

		goto END;
	}

	//======================================

	if (bad_ip_list.size()==0)
		goto END;

	// put ip of all devices into busy_list
	{
		QMutexLocker lock(&all_devices_mtx);

		foreach(CLDevice* dev, all_devices)
		{
			CLNetworkDevice* device = static_cast<CLNetworkDevice*>(dev);

			if (device->checkDeviceTypeFlag(CLDevice::NETWORK))
				busy_list.insert(device->getIP().toIPv4Address());

		}
	}

	foreach(CLDevice* dev, devices)
	{
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(dev);
		busy_list.insert(device->getIP().toIPv4Address());
	}

	//======================================

	time.restart();
	if (bad_ip_list.size())
		cl_log.log("Changing IP addresses... ", cl_logDEBUG1);

	resovle_conflicts(bad_ip_list, busy_list, ip_finished);

	if (bad_ip_list.size())
		cl_log.log("Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);

	fromListToList(bad_ip_list,  devices, 0, 0); // move everything to result list

END:

	if (!ip_finished && allow_to_change_ip)
	{
		// also in case if in already existing devices some devices conflicts with something see [-1-]
		// we need to resolve that conflicts

		CLDeviceList bad_ip_list;

		QMutexLocker lock(&all_devices_mtx);
		fromListToList(all_devices, bad_ip_list, CLDeviceStatus::NOT_IN_SUBNET, CLDeviceStatus::NOT_IN_SUBNET);
		if (bad_ip_list.size())
		{
			resovle_conflicts(bad_ip_list, busy_list, ip_finished);
			fromListToList(bad_ip_list, all_devices, 0, 0);
		}

	}

	// ok. at this point devices contains only network devices. and some of them have unknownDevice==true;
	// we need to resolve such devices 
	if (devices.count())
	{
		devices = resolveUnknown_helper(devices);
		CLDevice::getDevicesBasicInfo(devices, 4);

	}

	CLDevice::mergeLists(devices, not_network_devices); // move everything to result list

	return devices;

}
//====================================================================================

void CLDeviceSearcher::resovle_conflicts(CLDeviceList& device_list, CLIPList& busy_list, bool& ip_finished)
{
	foreach(CLDevice* dev, device_list)
	{
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(dev);

		if (!m_netState.existsSubnet(device->getDiscoveryAddr())) // very strange
			continue;

		CLSubNetState& subnet = m_netState.getSubNetState(device->getDiscoveryAddr());

		cl_log.log("Looking for next addr...", cl_logDEBUG1);

		if (!getNextAvailableAddr(subnet, busy_list))
		{
			ip_finished = true;			// no more FREE ip left ?
			cl_log.log("No more available IP!!", cl_logERROR);
			break;
		}

		if (device->setIP(subnet.currHostAddress, true))
		{
			device->getStatus().removeFlag(CLDeviceStatus::CONFLICTING);
			device->getStatus().removeFlag(CLDeviceStatus::NOT_IN_SUBNET);
		}

	}

}

bool CLDeviceSearcher::checkObviousConflicts(CLDeviceList& lst)
{
	// this function deals with network devices only 

	bool result = false;

	QMap<QString,  CLDevice*> ips;
	QMap<QString,  CLDevice*>::iterator ip_it;

	/**/
	{
		// put in already busy ip, ip from all_devices
		QMutexLocker lock(&all_devices_mtx);

		CLDeviceList::iterator it = all_devices.begin();
		while (it!=all_devices.end())
		{
			CLNetworkDevice* device = static_cast<CLNetworkDevice*>(it.value());
			if (device->checkDeviceTypeFlag(CLDevice::NETWORK))
				ips[device->getIP().toString()] = device;

			++it;
		}
	}
	/**/

	CLDeviceList::iterator it = lst.begin();
	while (it!=lst.end())
	{
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(it.value());

		QString ip = device->getIP().toString();
		ip_it = ips.find(ip);

		if (ip_it == ips.end())
			ips[ip] = it.value();
		else
		{
			ip_it.value()->getStatus().setFlag(CLDeviceStatus::CONFLICTING );
			it.value()->getStatus().setFlag(CLDeviceStatus::CONFLICTING );

			result = true;
		}

		++it;
	}

	return result;
}

void CLDeviceSearcher::fromListToList(CLDeviceList& from, CLDeviceList& to, int mask, int value)
{
	CLDeviceList::iterator it = from.begin();
	while (it!=from.end())
	{
		CLDevice* device = it.value();

		if (device->getStatus().checkFlag(mask)==value)
		{
			to[device->getUniqueId()] = device;
			from.erase(it++);
		}
		else
			++it;
	}

}

#ifndef _WIN32
struct T
{
	T(CLNetworkDevice* d)
	{
		device = d;
	}
	
	void f()
	{
		device->conflicting();
	}
	
	CLNetworkDevice* device;
};
#endif

void CLDeviceSearcher::markConflictingDevices(CLDeviceList& lst, int threads)
{
	// cannot make concurrent work with pointer CLDevice* ; => so extra steps needed
	// this function deals with network devices only 

#ifdef _WIN32
    struct T
    {
        T(CLNetworkDevice* d)
        {
            device = d;
        }

        void f()
        {
            device->conflicting();
        }

        CLNetworkDevice* device;
    };
#endif


	QList<T> local_list;

	CLDeviceList::iterator it = lst.begin();
	while(it!=lst.end())
	{
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(it.value());
		local_list.push_back(T(device));
		++it;
	}

	QThreadPool* global = QThreadPool::globalInstance();

	for (int i = 0; i < threads; ++i ) global->releaseThread();
	QtConcurrent::blockingMap(local_list, &T::f);
	for (int i = 0; i < threads; ++i )global->reserveThread();

}

CLDeviceList CLDeviceSearcher::resolveUnknown_helper(CLDeviceList& lst)
{

	CLDeviceList result;

	CLDeviceList::iterator it = lst.begin();
	while (it!=lst.end())
	{
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(it.value());

		if (!device->unknownDevice() || 
			device->getStatus().checkFlag(CLDeviceStatus::CONFLICTING) ||
			device->getStatus().checkFlag(CLDeviceStatus::NOT_IN_SUBNET))
		{
			// if this is not unknown device or if we cannot access it anyway
			result[device->getUniqueId()] = device;
			++it;
			continue;
		}

		// device is unknown and not conflicting 
		CLNetworkDevice* new_device = device->updateDevice();

		if (new_device)
		{
			// device updated
			result[new_device->getUniqueId()] = new_device;
			device->releaseRef();
		}
		else
		{
			// must be we still can not access to this device
			device->getStatus().setFlag(CLDeviceStatus::CONFLICTING);
			result[device->getUniqueId()] = device;
		}

		++it;
	}

	return result;

}
