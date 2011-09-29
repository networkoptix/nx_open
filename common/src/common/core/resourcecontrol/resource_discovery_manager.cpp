#include "resource_discovery_manager.h"

#include <QtCore/QtConcurrentMap>
#include <QtCore/QThreadPool>

#include "resource/network_resource.h"
#include "abstract_resource_searcher.h"
#include "resource_pool.h"

QnResourceDiscoveryManager::QnResourceDiscoveryManager()
{
}

QnResourceDiscoveryManager::~QnResourceDiscoveryManager()
{
	wait();
}

void QnResourceDiscoveryManager::addDeviceServer(QnAbstractResourceSearcher* serv)
{
	QMutexLocker lock(&m_searchersListMtx);
	m_searchersList.push_back(serv);
}

QnResourceList QnResourceDiscoveryManager::result()
{
	return m_result;
}

void QnResourceDiscoveryManager::run()
{
	bool ip_finished;
	m_result = findNewResources(ip_finished);

	if (ip_finished)
	{
		static bool first_time = true;
		const QString& message = QObject::trUtf8("Cannot get available IP address.");
		CL_LOG(cl_logWARNING) cl_log.log(message ,cl_logWARNING);
		if (first_time)
		{
			// msg box was here
			first_time = false;
		}
	}

}

QnResourceList QnResourceDiscoveryManager::findNewResources(bool& ip_finished)
{
    /*/
    bool allow_to_change_ip = allowToChangeresourceIP;

    ip_finished = false;

	QnResourceList::iterator it;
	QTime time;
	time.start();

	m_netState.updateNetState(); // update net state before serach

	//====================================
	CL_LOG(cl_logDEBUG1) cl_log.log("looking for resources...", cl_logDEBUG1);

	QnResourceList resources;


	{
		QMutexLocker lock(&m_searchersListMtx);
		foreach(QnAbstractResourceSearcher* searcher, m_searchersList)
		{
			QnResourceList temp = searcher->findResources();
			resources.append(temp);
		}

	}
	//

	//excluding already existing resources with READY status
	{

		it = resources.begin();
		while (it!=resources.end())
		{

			QnResourcePtr existingResource = QnResourcePool::instance().hasEqualResource(*it);

			if (!existingResource)
			{
				// new one; sholud stay
				++it;
				continue;
			}

			if (existingResource->getStatus().checkFlag(QnResourceStatus::READY)) // such resource already exists
			{
				// however, ip address or mask may be changed( very unlikely but who knows )
				// and this is why we need to check if old( already existing resource) still is in our subnet
				if (existingResource->checkDeviceTypeFlag(QnResource::NETWORK))
				{
					QnNetworkResourcePtr existingResourceNet = existingResource.staticCast<QnNetworkResource>();
					if (!m_netState.isInMachineSubnet(existingResourceNet->getHostAddress())) // not the same network
					{
						//[-1-]
						// we do changed ip address or subnet mask or even removed old nic interface and likely this resource has "lost connection status"
						// ip address must be changed

						QnNetworkResourcePtr newResourceNet = (*it).staticCast<QnNetworkResource>();
						existingResourceNet->setDiscoveryAddr(newResourceNet->getDiscoveryAddr()); // update DiscoveryAddr; it will help to find new available ip in subnet
						existingResourceNet->getStatus().setFlag(QnResourceStatus::NOT_IN_SUBNET);
					}

				}

				it = resources.erase(it);
			}
			else
				++it; // new resource; must stay

		}

	}
	//====================================
	// at this point in resources we have all new found resources
	QnResourceList notNetworkResources;

	// remove all not network resources from list
	{
		QnResourceList::iterator it = resources.begin();
		while (it!=resources.end())
		{

			if (!(*it).dynamicCast<QnNetworkResource>()) // is it network resource
			{
				notNetworkResources.push_back(*it);
				it = resources.erase(it);
			}
			else
				++it;
		}
	}

	// now resources list has only network resources

	// lets form the list of existing IP
	CLIPList busy_list;

	QnResourceList bad_ip_list;

	cl_log.log("Found ", resources.size() + notNetworkResources.size(), " new(!) resources.", cl_logDEBUG1);

	CL_LOG(cl_logDEBUG2)
	{
		foreach(QnResourcePtr res, resources)
			cl_log.log(res->toString(), cl_logDEBUG2);
	}

	cl_log.log("Time elapsed: ", time.elapsed(), cl_logDEBUG1);

	if (resources.size()==0) // no new resources
		goto END;

	//====================================
	checkObviousConflicts(resources); // if conflicting it will mark it
	//====================================

	// check if all in our subnet

	foreach(QnResourcePtr res, resources)
	{
		QnNetworkResourcePtr resource = res.staticCast<QnNetworkResource>();

		if ((!resource->isAfterRouter()) && (!m_netState.isInMachineSubnet(resource->getHostAddress()))) // not the same network
			resource->getStatus().setFlag(QnResourceStatus::NOT_IN_SUBNET);
	}

	//=====================================

	// move all conflicting cams and cams with bad ip to bad_ip_list
	fromListToList(resources, bad_ip_list, QnResourceStatus::NOT_IN_SUBNET, QnResourceStatus::NOT_IN_SUBNET);
	fromListToList(resources, bad_ip_list, QnResourceStatus::CONFLICTING, QnResourceStatus::CONFLICTING);

	time.restart();

	cl_log.log("Checking for real conflicts ", resources.size(), " resources.", cl_logDEBUG1);
	markConflictingResources(resources,5);
	fromListToList(resources, bad_ip_list, QnResourceStatus::CONFLICTING, QnResourceStatus::CONFLICTING);
	cl_log.log("Time elapsed ", time.restart(), cl_logDEBUG1);
	cl_log.log(" ", resources.size(), " new(!) resources not conflicting .", cl_logDEBUG1);

	//======================================

	// now in resources only new non conflicting resources; in bad_ip_list only resources with conflicts, so ip of bad_ip_list must be chnged
	if (!allow_to_change_ip)// nothing else we can do
	{
		// move all back to resources
		fromListToList(bad_ip_list, resources,  QnResourceStatus::CONFLICTING, QnResourceStatus::CONFLICTING);
		fromListToList(bad_ip_list, resources, QnResourceStatus::NOT_IN_SUBNET, QnResourceStatus::NOT_IN_SUBNET);

		goto END;
	}

	//======================================

	if (bad_ip_list.size()==0)
		goto END;

	// put ip of all resources into busy_list
	{

		QnResourceList networkResources = QnResourcePool::instance().getResourcesWithFlag(QnResource::NETWORK);

		foreach(QnResourcePtr res, networkResources)
		{
			QnNetworkResourcePtr netResource = res.staticCast<QnNetworkResource>();
			busy_list.insert(netResource->getHostAddress().toIPv4Address());
		}
	}

	foreach(QnResourcePtr res, resources)
	{
		QnNetworkResourcePtr netResource = res.staticCast<QnNetworkResource>();
		busy_list.insert(netResource->getHostAddress().toIPv4Address());
	}

	//======================================

	time.restart();
	if (bad_ip_list.size())
		cl_log.log("Changing IP addresses... ", cl_logDEBUG1);

	resovle_conflicts(bad_ip_list, busy_list, ip_finished);

	if (bad_ip_list.size())
		cl_log.log("Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);

	fromListToList(bad_ip_list,  resources, 0, 0); // move everything to result list

END:

	if (!ip_finished && allow_to_change_ip)
	{
		// also in case if in already existing resources some resources conflicts with something see [-1-]
		// we need to resolve that conflicts


		QnResourceList bad_ip_list;

		QMutexLocker lock(&all_devices_mtx);
		fromListToList(all_devices, bad_ip_list, QnResourceStatus::NOT_IN_SUBNET, QnResourceStatus::NOT_IN_SUBNET);
		if (bad_ip_list.size())
		{
			resovle_conflicts(bad_ip_list, busy_list, ip_finished);
			fromListToList(bad_ip_list, all_devices, 0, 0);
		}


	}

	// ok. at this point resources contains only network resources. and some of them have unknownResource==true;
	// we need to resolve such resources
	if (resources.count())
	{
		resources = resolveUnknown_helper(resources);
		QnResource::getDevicesBasicInfo(resources, 4);

	}

	resources.append(notNetworkResources); // move everything to result list

	/*/

	QnResourceList resources;

	return resources;
}

//====================================================================================

void QnResourceDiscoveryManager::resovle_conflicts(QnResourceList& resourceList, CLIPList& busy_list, bool& ip_finished)
{
	foreach(QnResourcePtr res, resourceList)
	{
		QnNetworkResourcePtr resource = res.dynamicCast<QnNetworkResource>();

		if (!m_netState.existsSubnet(resource->getDiscoveryAddr())) // very strange
			continue;

		CLSubNetState& subnet = m_netState.getSubNetState(resource->getDiscoveryAddr());

		cl_log.log("Looking for next addr...", cl_logDEBUG1);

		if (!getNextAvailableAddr(subnet, busy_list))
		{
			ip_finished = true;			// no more FREE ip left ?
			cl_log.log("No more available IP!!", cl_logERROR);
			break;
		}

		if (resource->setHostAddress(subnet.currHostAddress, QnDomainPhysical))
		{
			//todo
			//resource->getStatus().removeFlag(QnResourceStatus::CONFLICTING);
			//resource->getStatus().removeFlag(QnResourceStatus::NOT_IN_SUBNET);
		}

	}

}

bool QnResourceDiscoveryManager::checkObviousConflicts(QnResourceList& lst)
{
    // this function deals with network resources only

    /*/

    bool result = false;

	QMap<QString,  QnResource*> ips;
	QMap<QString,  QnResource*>::iterator ip_it;


	{
		// put in already busy ip, ip from all_devices
		QMutexLocker lock(&all_devices_mtx);

		QnResourceList::iterator it = all_devices.begin();
		while (it!=all_devices.end())
		{
			QnNetworkResource* resource = static_cast<QnNetworkResource*>(it.value());
			if (resource->checkDeviceTypeFlag(QnResource::NETWORK))
				ips[resource->getHostAddress().toString()] = resource;

			++it;
		}
	}


	QnResourceList::iterator it = lst.begin();
	while (it!=lst.end())
	{
		QnNetworkResource* resource = static_cast<QnNetworkResource*>(it.value());

		QString ip = resource->getHostAddress().toString();
		ip_it = ips.find(ip);

		if (ip_it == ips.end())
			ips[ip] = it.value();
		else
		{
			ip_it.value()->getStatus().setFlag(QnResourceStatus::CONFLICTING );
			it.value()->getStatus().setFlag(QnResourceStatus::CONFLICTING );

			result = true;
		}

		++it;
	}

	return result;

    /*/
    return true;
}

void QnResourceDiscoveryManager::fromListToList(QnResourceList& from, QnResourceList& to, int mask, int value)
{
	/*
	QnResourceList::iterator it = from.begin();
	while (it!=from.end())
	{
		QnResource* resource = it.value();

		if (resource->getStatus().checkFlag(mask)==value)
		{
			to[resource->getUniqueId()] = resource;
			from.erase(it++);
		}
		else
			++it;
	}
	/*/
}

struct T
{
	T(QnNetworkResourcePtr r)
	{
		resource = r;
	}

	void f()
	{
		resource->conflicting();
	}

	QnNetworkResourcePtr resource;
};

void QnResourceDiscoveryManager::markConflictingResources(QnResourceList& lst, int threads)
{
	// cannot make concurrent work with pointer CLDevice* ; => so extra steps needed
	// this function deals with network resources only

	QList<T> local_list;

    foreach(QnResourcePtr res, lst)
    {
        QnNetworkResourcePtr resource = res.dynamicCast<QnNetworkResource>();
        local_list.push_back(T(resource));
    }


	QThreadPool* global = QThreadPool::globalInstance();

	for (int i = 0; i < threads; ++i ) global->releaseThread();
	QtConcurrent::blockingMap(local_list, &T::f);
	for (int i = 0; i < threads; ++i )global->reserveThread();

}

QnResourceList QnResourceDiscoveryManager::resolveUnknown_helper(QnResourceList& lst)
{

    QnResourceList result;

    /*/todo
    foreach(QnResourcePtr res, lst)
    {
        QnNetworkResourcePtr resource = res.staticCast<QnNetworkResource>();

		if (!resource->unknownResource() ||
			resource->getStatus().checkFlag(QnResourceStatus::CONFLICTING) ||
			resource->getStatus().checkFlag(QnResourceStatus::NOT_IN_SUBNET))
		{
			// if this is not unknown resource or if we cannot access it anyway
			result.push_back(resource);
			continue;
		}

		// resource is unknown and not conflicting
		QnNetworkResourcePtr newResource = resource->updateResource();

		if (newResource)
		{
			// resource updated
			result.push_back(newResource);
			//resource->releaseRef();
		}
		else
		{
			// must be we still can not access to this resource
			resource->getStatus().setFlag(QnResourceStatus::CONFLICTING);
			result.push_back(resource);
		}

    }
    /*/

    return result;
}
