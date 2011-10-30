#include <QtCore/QtConcurrentMap>
#include <QtCore/QThreadPool>
#include "asynch_seacher.h"
#include "utils/common/sleep.h"
#include "resourceserver.h"
#include "../resource/network_resource.h"
#include "resource_pool.h"




QnResourceDiscoveryManager::QnResourceDiscoveryManager()
{
}

QnResourceDiscoveryManager::~QnResourceDiscoveryManager()
{
	stop();
}

QnResourceDiscoveryManager& QnResourceDiscoveryManager::instance()
{
    static QnResourceDiscoveryManager inst;
    return inst;
}

void QnResourceDiscoveryManager::addDeviceServer(QnAbstractResourceSearcher* serv)
{
	QMutexLocker lock(&m_searchersListMtx);
	m_searchersList.push_back(serv);
}

void QnResourceDiscoveryManager::pleaseStop()
{
    {
        QMutexLocker lock(&m_searchersListMtx);
        foreach(QnAbstractResourceSearcher* searcher, m_searchersList)
        {
            searcher->pleaseStop();
        }
    }

    CLLongRunnable::pleaseStop();
}

void QnResourceDiscoveryManager::run()
{
	while(!needToStop())
    {
        bool ip_finished;
        QnResourceList result = findNewResources(ip_finished);

        if (ip_finished)
        {
            const QString& message = QObject::trUtf8("Cannot get available IP address.");
            CL_LOG(cl_logWARNING) cl_log.log(message ,cl_logWARNING);
        }

        QnResourcePool::instance()->addResources(result);


        int global_delay_between_search = 1000;
        smartSleep(global_delay_between_search);

    }
}

QnResourceList QnResourceDiscoveryManager::findNewResources(bool& ip_finished)
{
    
    //bool allow_to_change_ip = true;
    static const int  threads = 5;

    ip_finished = false;

	QTime time;
	time.start();

	m_netState.updateNetState(); // update net state before search

	//====================================
	CL_LOG(cl_logDEBUG1) cl_log.log("looking for resources...", cl_logDEBUG1);

	QnResourceList resources;
    QnResourceList::iterator it;


    ResourceSearcherList searchersList;
    {
        QMutexLocker lock(&m_searchersListMtx);
        searchersList = m_searchersList;
    }

		
	foreach(QnAbstractResourceSearcher* searcher, searchersList)
	{
        if (searcher->shouldBeUsed() && !needToStop())
        {
		    QnResourceList temp = searcher->findResources();
		    resources.append(temp);
        }
	}

    //assemble list of existing ip

    // from pool
    QMap<quint32, int> ipsList;  
    QnResourceList resList = QnResourcePool::instance()->getResourcesWithFlag(QnResource::network);
    foreach(QnResourcePtr res, resList)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes)
        {
            quint32 ips = netRes->getHostAddress().toIPv4Address();
            if (ipsList.contains(ips))
                ipsList[ips]++;
            else
                ipsList[ips] = 1;
        }
    }

    // from just found 
    foreach(QnResourcePtr res, resources)
    {
        if (QnResourcePool::instance()->hasSuchResouce( res->getUniqueId() ))
            continue; // this ip is already taken into account 

        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes)
        {
            quint32 ips = netRes->getHostAddress().toIPv4Address();
            if (ipsList.contains(ips))
                ipsList[ips]++;
            else
                ipsList[ips] = 1;
        }
    }

    // now let's mark conflicting resources( just new )
    // in pool could not be 2 resources with same ip
    foreach(QnResourcePtr res, resources)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes)
        {
            quint32 ips = netRes->getHostAddress().toIPv4Address();
            if (ipsList.count(ips)>1)
                netRes->setNetworkStatus(QnNetworkResource::BadHostAddr);
        }
    }



	
	//marks all new network resources as badip if not accessible. 
    //if resources are present in resourcepull already it marks as badip as well
    check_if_accessible(resources, threads);

    
    //excluding already existing resources 
    it = resources.begin();
    while (it!=resources.end())
    {
        if (QnResourcePool::instance()->hasSuchResouce( (*it)->getUniqueId() ))
            it = resources.erase(it);
        else
            ++it;
    }
            
    //========================================================
    // now we've got only new resources.
    // let's assemble list of not accessible network resources
    QnResourceList readyToGo; // list of any new non conflicting resources
    it = resources.begin();
    while (it!=resources.end())
    {
        QnNetworkResourcePtr netRes = (*it).dynamicCast<QnNetworkResource>();
        if (netRes && netRes->checkNetworkStatus(QnNetworkResource::BadHostAddr)) // if this is network resource and it has bad ip should stay
        {
            ++it;
            continue;
        }

        readyToGo.push_back(*it);
        it =  resources.erase(it);
    }


    // readyToGo contains ready to go resources 
    // resources contains only new network conflicting resources 
    
	// now resources list has only network resources

	cl_log.log("Found ", resources.size(), " conflicting resources.", cl_logDEBUG1);
	cl_log.log("Time elapsed: ", time.elapsed(), cl_logDEBUG1);
	//======================================

	time.restart();
	if (resources.size())
		cl_log.log("Changing IP addresses... ", cl_logDEBUG1);

	resovle_conflicts(resources, ipsList.keys(), ip_finished);

	if (resources.size())
		cl_log.log("Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);


    // lets remove still not accessible resources 
    it = resources.begin();
    while (it!=resources.end())
    {
        QnNetworkResourcePtr netRes = (*it).dynamicCast<QnNetworkResource>();
        if (netRes && netRes->checkNetworkStatus(QnNetworkResource::BadHostAddr)) // if this is network resource and it has bad ip should stay
        {
            it =  resources.erase(it);
            continue;
        }

        ++it;
    }


    // at this point lets combine back all resources
    foreach(QnResourcePtr res, readyToGo)
    {
        resources.push_back(res);
    }



    QnResourceList swapList;
    foreach(QnResourcePtr res, resources)
    {
        if (res->unknownResource())
        {
            QnResourcePtr updetedRes = res->updateResource();
            swapList.push_back(updetedRes);
        }
        else
            swapList.push_back(res);
    }


	return resources;
}

//==========================check_if_accessible========================
struct check_if_accessible_STRUCT
{

    QnNetworkResourcePtr resourceNet;
    bool m_isSameSubnet;

    check_if_accessible_STRUCT(QnNetworkResourcePtr res, bool isSameSubnet):
    m_isSameSubnet(isSameSubnet)
    {
        resourceNet = res;
    }

    void f()
    {
        bool acc = resourceNet->checkFlag(QnNetworkResource::BadHostAddr); // bad ip already 

        if (acc)
        {
            if (m_isSameSubnet) // if same subnet
                acc = !(resourceNet->conflicting());
            else
                acc = resourceNet->isResourceAccessible();
        }

            
        QnResourcePtr existingResource = QnResourcePool::instance()->getResourceByUniqId( resourceNet->getUniqueId() );
        if (existingResource)
        {
            // if such resource already exists; in the pool we should update acc flag
            resourceNet = existingResource.dynamicCast<QnNetworkResource>();
        }

        if (acc)
            resourceNet->removeNetworkStatus(QnNetworkResource::BadHostAddr);
        else
            resourceNet->addNetworkStatus(QnNetworkResource::BadHostAddr);


    }
};


void QnResourceDiscoveryManager::check_if_accessible(QnResourceList& justfoundList, int threads)
{
    QList<check_if_accessible_STRUCT> checkLst;

    foreach(QnResourcePtr res, justfoundList)
    {
        QnNetworkResourcePtr nr = res.dynamicCast<QnNetworkResource>();

        if (!nr)
            continue;

        check_if_accessible_STRUCT t(nr, m_netState.isInMachineSubnet(nr->getHostAddress()) );
        checkLst.push_back(t);
    }

    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) global->releaseThread();
    QtConcurrent::blockingMap(checkLst, &check_if_accessible_STRUCT::f);
    for (int i = 0; i < threads; ++i )global->reserveThread();

}

//====================================================================================

void QnResourceDiscoveryManager::resovle_conflicts(QnResourceList& resourceList, const CLIPList& busy_list, bool& ip_finished)
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

		if (resource->setHostAddress(subnet.currHostAddress, QnDomainPhysical) && resource->isResourceAccessible())
		{
            resource->removeNetworkStatus(QnNetworkResource::BadHostAddr);
		}
	}
}

