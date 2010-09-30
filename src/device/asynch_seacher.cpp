#include "asynch_seacher.h"
#include "../base/log.h"
#include <QMessageBox>
#include <QTime>
#include "deviceserver.h"
#include "../network/nettools.h"
#include <QThreadPool>
#include <QtConcurrentmap>
#include "network_device.h"

CLDiviceSeracher::CLDiviceSeracher(bool allow_change_ip):
m_allow_change_ip(allow_change_ip)
{

}

CLDiviceSeracher::~CLDiviceSeracher()
{
	wait();
}

void CLDiviceSeracher::addDeviceServer(CLDeviceServer* serv)
{
	QMutexLocker lock(&m_servers_mtx);
	m_servers.push_back(serv);
}

CLDeviceList CLDiviceSeracher::result()
{
	return m_result;
}

void CLDiviceSeracher::run()
{
	bool ip_finished;
	m_result = findNewDevices(m_allow_change_ip, ip_finished);

	if (ip_finished)
	{
		static bool first_time = true;
		const QString& message = QObject::trUtf8("Cannot get free IP address.");
		CL_LOG(cl_logWARNING) cl_log.log(message ,cl_logWARNING);
		if (first_time)
		{
			QMessageBox* box = new QMessageBox(QMessageBox::Warning, "Info", message, QMessageBox::Ok, 0);
			box->show();
			first_time = false;
		}
	}
	
}


CLDeviceList CLDiviceSeracher::findNewDevices(bool allow_to_change_ip, bool& ip_finished)
{
	ip_finished = false;

	CLDeviceList::iterator it;
	QTime time;
	time.start();

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
			if (all_dev->getStatus().checkFlag(CLDeviceStatus::READY)) // such device already exists and in a good shape
			{
				//delete it.value(); //here+
				it.value()->releaseRef();
				devices.erase(it++);
			}
			else
				++it; // new device; must stay 
		}

	}
	//====================================
	// at this point in devices we have all found devices
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


	//return devices; //<- this line should be removed if you do not deal with fake devices. I'll fix it shortly

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
	it = devices.begin(); 
	while (it!=devices.end())
	{
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(it.value());

		if (!m_netState.isInMachineSubnet(device->getIP())) // not the same network
			device->getStatus().setFlag(CLDeviceStatus::NOT_IN_SUBNET);

		++it;
	}
	//=====================================


	

	// move all conflicting cams and cams with bad ip to bad_ip_list
	fromListToList(devices, bad_ip_list, CLDeviceStatus::NOT_IN_SUBNET, CLDeviceStatus::NOT_IN_SUBNET);
	fromListToList(devices, bad_ip_list, CLDeviceStatus::CONFLICTING, CLDeviceStatus::CONFLICTING);


	time.restart();

	cl_log.log("Checking for real conflicts ", devices.size(), " devices.", cl_logDEBUG1);
	markConflictingDevices(devices,10);
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

		it = all_devices.begin();
		while (it!=all_devices.end())
		{
			CLNetworkDevice* device = static_cast<CLNetworkDevice*>(it.value());

			if (device->checkDeviceTypeFlag(CLDevice::NETWORK))
				busy_list.insert(device->getIP().toIPv4Address());


			++it;
		}

	}

	it = devices.begin();
	while (it!=devices.end())
	{
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(it.value());
		busy_list.insert(device->getIP().toIPv4Address());
		++it;
	}
	//======================================

	time.restart();
	if (bad_ip_list.size())
		cl_log.log("Changing IP addresses... ", cl_logDEBUG1);

	it = bad_ip_list.begin();
	while(it!=bad_ip_list.end())
	{
		
		CLNetworkDevice* device = static_cast<CLNetworkDevice*>(it.value());
		

		if (!m_netState.exists(device->getDiscoveryAddr())) // very strange
		{
			++it;
			continue;
		}

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

		++it;
	}


	if (bad_ip_list.size())
		cl_log.log("Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);

	fromListToList(bad_ip_list,  devices, 0, 0); // move everything to result list

END:

	// ok. at this point devices contains only network devices. and some of them have unknownDevice==true;
	// we need to resolve such devices 
	devices = resolveUnknown_helper(devices);

	CLDevice::mergeLists(devices, not_network_devices); // move everything to result list

	return devices;

}

bool CLDiviceSeracher::checkObviousConflicts(CLDeviceList& lst)
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

void CLDiviceSeracher::fromListToList(CLDeviceList& from, CLDeviceList& to, int mask, int value)
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

void CLDiviceSeracher::markConflictingDevices(CLDeviceList& lst, int threads)
{
	// cannot make concurrent work with pointer CLDevice* ; => so extra steps needed
	// this function deals with network devices only 

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

CLDeviceList CLDiviceSeracher::resolveUnknown_helper(CLDeviceList& lst)
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