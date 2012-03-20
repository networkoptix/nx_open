#include <QtCore/QtConcurrentMap>
#include <QtCore/QThreadPool>
#include "resource_discovery_manager.h"
#include "utils/common/sleep.h"
#include "resource_searcher.h"
#include "../resource/network_resource.h"
#include "resource_pool.h"
#include "utils/common/util.h"
#include "api/AppServerConnection.h"
#include "core/resource/resource_directory_browser.h"
#include "utils/common/synctime.h"

namespace {
    class QnResourceDiscoveryManagerInstance: public QnResourceDiscoveryManager {};

    Q_GLOBAL_STATIC(QnResourceDiscoveryManagerInstance, qnResourceDiscoveryManagerInstance);
}

QnResourceDiscoveryManager::QnResourceDiscoveryManager():
    m_server(false),
    m_foundSmth(true),
    m_ready(false)
{
}

QnResourceDiscoveryManager::~QnResourceDiscoveryManager()
{
    stop();
}

void QnResourceDiscoveryManager::setReady(bool ready) 
{
    m_ready = ready;
}

QnResourceDiscoveryManager& QnResourceDiscoveryManager::instance()
{
    return *qnResourceDiscoveryManagerInstance();
}

void QnResourceDiscoveryManager::setServer(bool serv)
{
    m_server = serv;
}

bool QnResourceDiscoveryManager::isServer() const
{
    return m_server;
}

void QnResourceDiscoveryManager::addDeviceServer(QnAbstractResourceSearcher* serv)
{
    QMutexLocker locker(&m_searchersListMutex);
    m_searchersList.push_back(serv);
}

void QnResourceDiscoveryManager::setResourceProcessor(QnResourceProcessor* processor)
{
    QMutexLocker locker(&m_searchersListMutex);
    m_resourceProcessor = processor;
}

QnResourcePtr QnResourceDiscoveryManager::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnResourcePtr result;

    ResourceSearcherList searchersList;
    {
        QMutexLocker locker(&m_searchersListMutex);
        searchersList = m_searchersList;
    }
    foreach (QnAbstractResourceSearcher *searcher, searchersList)
    {
        result = searcher->createResource(resourceTypeId, parameters);
        if (!result.isNull())
            break;
    }

    return result;
}

void QnResourceDiscoveryManager::pleaseStop()
{
    {
        QMutexLocker locker(&m_searchersListMutex);
        foreach (QnAbstractResourceSearcher *searcher, m_searchersList)
            searcher->pleaseStop();
    }

    CLLongRunnable::pleaseStop();
}

void QnResourceDiscoveryManager::run()
{
    ResourceSearcherList searchersList;
    {
        QMutexLocker locker(&m_searchersListMutex);
        searchersList = m_searchersList;
    }

    foreach (QnAbstractResourceSearcher *searcher, searchersList)
    {
        searcher->setShouldBeUsed(true);

        if (QnResourceDirectoryBrowser* ds = dynamic_cast<QnResourceDirectoryBrowser*>(searcher))
        {
            QnResourceList lst = searcher->search();
            m_resourceProcessor->processResources(lst);
        }
    }

    while (!needToStop() && !m_ready)
        QnSleep::msleep(1);

    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();
    while (!needToStop() && !initResourceTypes(appServerConnection))
    {
        QnSleep::msleep(1000);
    }

    while (!needToStop() && !initLicenses(appServerConnection))
    {
        QnSleep::msleep(1000);
    }

    while (!needToStop())
    {
        bool ip_finished;
        QnResourceList result = findNewResources(&ip_finished);
        if (ip_finished)
            CL_LOG(cl_logWARNING) cl_log.log(QLatin1String("Cannot get available IP address."), cl_logWARNING);

        if (!result.isEmpty())
        {
            m_resourceProcessor->processResources(result);
        }

        int global_delay_between_search = 1000;
        smartSleep(global_delay_between_search);
    }
}

void printInLogNetResources(const QnResourceList& resources)
{
    foreach(QnResourcePtr res, resources)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (!netRes)
            continue;

        cl_log.log(netRes->getHostAddress().toString() + QString(" "), netRes->getName(), cl_logALWAYS);
    }

}

QnResourceList QnResourceDiscoveryManager::findNewResources(bool *ip_finished)
{
    //bool allow_to_change_ip = true;
    static const int  threads = 5;

    *ip_finished = false;

    QTime time;
    time.start();

    if (m_foundSmth)
        cl_log.log("looking for resources ===========...", cl_logALWAYS);

    QnResourceList resources;
    QnResourceList::iterator it;


    ResourceSearcherList searchersList;
    {
        QMutexLocker locker(&m_searchersListMutex);
        searchersList = m_searchersList;
    }

    foreach (QnAbstractResourceSearcher *searcher, searchersList)
    {
        if (searcher->shouldBeUsed() && !needToStop())
        {
            QnResourceList lst = searcher->search();

            // resources can be searched by client in or server.
            // if this client in stand alone => lets add QnResource::local 
            // server does not care about flags 
            foreach (const QnResourcePtr &res, lst)
            {
                if (searcher->isLocal())
                    res->addFlags(QnResource::local);
            }

            resources.append(lst);
        }
    }


    //assemble list of existing ip
    QMap<quint32, int> ipsList;


    //excluding already existing resources
    it = resources.begin();
    while (it != resources.end())
    {
        QnResourcePtr rpResource = qnResPool->getResourceByUniqId((*it)->getUniqueId());
        QnNetworkResourcePtr rpNetRes = rpResource.dynamicCast<QnNetworkResource>();

        if (rpResource)
        {
            // if such res in ResourcePool
            QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();

            if (newNetRes)
            {
                if (!newNetRes->checkFlags(QnResource::server_live_cam)) // if this is not camera from mediaserver on the client stand alone
                {
                    quint32 ips = newNetRes->getHostAddress().toIPv4Address();
                    if (ipsList.contains(ips))
                        ipsList[ips]++;
                    else
                        ipsList[ips] = 1;
                }


                
                bool diffAddr = rpNetRes && rpNetRes->getHostAddress() != newNetRes->getHostAddress(); //if such network resource is in pool and has diff IP 
                bool diffNet = !m_netState.isResourceInMachineSubnet(newNetRes->getHostAddress(), newNetRes->getDiscoveryAddr()); // or is diff subnet NET
                if ( diffAddr || diffNet)
                {
                    // should keep it into resources to investigate it further 
                    ++it;
                    continue;
                }
            }

            // seems like resource is in the pool and has OK ip
            updateResourceStatus(rpNetRes);

            it = resources.erase(it); // do not need to investigate OK resources


        }
        else
            ++it; // new resource => shouls keep it
    }

    //==========if resource is not discover last minute and we do not record it and do not see live => readers not runing 
    markOfflineIfNeeded();
    //======================

    if (resources.size())
    {
        cl_log.log("Discovery----: after excluding existing resources we've got ", resources.size(), " new resources:", cl_logALWAYS);
        m_foundSmth = true;
    }
    else
    {
        m_foundSmth = false;
    }
    
    printInLogNetResources(resources);

    if (resources.size()==0)
        return resources;


    // now let's mark conflicting resources( just new )
    // in pool could not be 2 resources with same ip
    foreach (const QnResourcePtr &res, resources)
    {
        if (res->checkFlags(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;

        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes && netRes->shoudResolveConflicts())
        {
            quint32 ips = netRes->getHostAddress().toIPv4Address();
            if (ipsList.count(ips) > 0 && ipsList[ips] > 1)
            {
                netRes->setNetworkStatus(QnNetworkResource::BadHostAddr);
                cl_log.log(netRes->getHostAddress().toString() , " conflicting. Has same IP as someone else.", cl_logALWAYS);
            }
        }
    }

    if (resources.size()) {
        m_netState.updateNetState(); // update net state before working with discovered resources
    }

    //marks all new network resources as badip if: 1) not in the same subnet and not accesible or 2) same subnet and conflicting
    cl_log.log("Discovery---- Checking if resources are accessible...", cl_logALWAYS);
    check_if_accessible(resources, threads);


    //========================================================
    // now we've got only new resources.
    // let's assemble list of not accessible network resources
    QnResourceList readyToGo; // list of any new non conflicting resources
    it = resources.begin();
    while (it != resources.end())
    {
        QnNetworkResourcePtr netRes = (*it).dynamicCast<QnNetworkResource>();
        if (netRes && netRes->checkNetworkStatus(QnNetworkResource::BadHostAddr)) // if this is network resource and it has bad ip should stay
        {
            ++it;
        }
        else
        {
            if (netRes)
                cl_log.log("Ready to go resource: ", netRes->getHostAddress().toString(), cl_logALWAYS);

            readyToGo.push_back(*it);
            it = resources.erase(it);
        }
    }


    // readyToGo contains ready to go resources
    // resources contains only new network conflicting resources
    // now resources list has only network resources

    cl_log.log("Discovery---- After check_if_accessible readyToGo List: ", cl_logALWAYS);
    printInLogNetResources(readyToGo);

    cl_log.log("Discovery---- After check_if_accessible conflicting List: ", cl_logALWAYS);
    printInLogNetResources(resources);
    

    //======================================

    time.restart();
    if (resources.size())
        cl_log.log("Discovery---- Changing IP addresses... ", cl_logDEBUG1);

    resovle_conflicts(resources, ipsList.keys(), ip_finished);

    if (resources.size())
        cl_log.log("Discovery---- Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);



    // lets remove still not accessible resources
    it = resources.begin();
    while (it != resources.end())
    {
        QnNetworkResourcePtr netRes = (*it).dynamicCast<QnNetworkResource>();
        if (netRes && netRes->checkNetworkStatus(QnNetworkResource::BadHostAddr)) // if this is network resource and it has bad ip should stay
        {
            it = resources.erase(it);
            cl_log.log("!!! Cannot resolve conflict for: ", netRes->getHostAddress().toString(), cl_logERROR);
        }
        else
            ++it;
    }


    // at this point lets combine back all resources
    foreach (const QnResourcePtr &res, readyToGo)
        resources.push_back(res);


    cl_log.log("Discovery---- After resovle_conflicts - list of non conflicting resource: ", cl_logALWAYS);
    printInLogNetResources(resources);



    QnResourceList swapList;
    foreach (const QnResourcePtr &res, resources)
    {
        if (res->unknownResource())
        {
            QnResourcePtr updetedRes = res->updateResource();
            if (updetedRes)
                swapList.push_back(updetedRes);
        }
        else
        {
            swapList.push_back(res);
        }
    }

    resources = swapList;

    cl_log.log("Discovery---- After update unknown - list of non conflicting resource: ", cl_logALWAYS);
    printInLogNetResources(resources);


    //and now lets correct the ip of already existing resources
    it = resources.begin();
    while (it != resources.end())
    {
        QnResourcePtr rpResource = qnResPool->getResourceByUniqId((*it)->getUniqueId());
        if (rpResource)
        {
            // if such res in ResourcePool
            QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();

            if (newNetRes)
            {
                QnNetworkResourcePtr rpNetRes = rpResource.dynamicCast<QnNetworkResource>();
                if (rpNetRes && rpNetRes->getHostAddress() != newNetRes->getHostAddress())
                {
                    // if such network resource is in pool and has diff IP => should keep it
                    rpNetRes->setHostAddress(newNetRes->getHostAddress(), QnDomainMemory);
                    it = resources.erase(it);

                    if (isServer())
                    {
                        // not stand alone
                        QnVirtualCameraResourcePtr cameraResource = rpNetRes.dynamicCast<QnVirtualCameraResource>();
                        if (cameraResource)
                        {
                            QByteArray errorString;
                            QnVirtualCameraResourceList cameras;
                            QnAppServerConnectionPtr connect = QnAppServerConnectionFactory::createConnection();
                            if (connect->addCamera(cameraResource, cameras, errorString) != 0)
                            {
                                qDebug() << "QnResourceDiscoveryManager::findNewResources(): Can't add camera. Reason: " << errorString;
                            }
                        }

                    }

                    continue;
                }
                else
                {
                    if (rpNetRes)
                        updateResourceStatus(rpNetRes);

                    //Q_ASSERT(false);
                    ++it;
                    continue;
                }
            }

            Q_ASSERT(false);


        }
        else
            ++it; // nothing to do
    }

    if (resources.size())
    {
        cl_log.log("Discovery---- Final result: ", cl_logALWAYS);
        printInLogNetResources(resources);
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
        bool acc = !resourceNet->checkNetworkStatus(QnNetworkResource::BadHostAddr); // not  bad ip so far

        if (acc)
        {
            if (m_isSameSubnet) // if same subnet
                acc = !(resourceNet->conflicting());
            else
                acc = resourceNet->isResourceAccessible();
        }


        if (!acc)
        {
            resourceNet->addNetworkStatus(QnNetworkResource::BadHostAddr);

            if (m_isSameSubnet)
                cl_log.log(resourceNet->getHostAddress().toString() + QString("  name = ") +  resourceNet->getName(), " has bad IP(same subnet)", cl_logALWAYS);
            else
                cl_log.log(resourceNet->getHostAddress().toString() + QString("  name = ") +  resourceNet->getName(), " has bad IP(diff subnet)", cl_logALWAYS);
        }

    }
};

void QnResourceDiscoveryManager::markOfflineIfNeeded()
{
    QnResourceList resources = qnResPool->getResources();

    foreach(QnResourcePtr res, resources)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (!netRes)
            continue;

        if (res->checkFlags(QnResource::server_live_cam)) // if this is camera from mediaserver on the client
            continue;

        QDateTime ldt = netRes->getLastDiscoveredTime();
        QDateTime currentT = qnSyncTime->currentDateTime();

        if (ldt.secsTo(currentT) > 30 && !netRes->hasRunningLiveProvider()) // if resource is not discovered last 30 sec
        {
			res->setStatus(QnResource::Offline);
        }

    }
    
}

void QnResourceDiscoveryManager::updateResourceStatus(QnResourcePtr res)
{
    // seems like resource is in the pool and has OK ip
    QnNetworkResourcePtr rpNetRes = res.dynamicCast<QnNetworkResource>();

    if (rpNetRes)
    {
        if (rpNetRes->getStatus() == QnResource::Offline) 
        {
            if (rpNetRes->getLastStatusUpdateTime().msecsTo(qnSyncTime->currentDateTime()) > 30) // if resource with OK ip seems to be found; I do it coz if there is no readers and camera was offline and now online => status needs to be changed
				rpNetRes->setStatus(QnResource::Online);

        }


        rpNetRes->setLastDiscoveredTime(qnSyncTime->currentDateTime());
    }

}

void QnResourceDiscoveryManager::check_if_accessible(QnResourceList& justfoundList, int threads)
{
    QList<check_if_accessible_STRUCT> checkLst;

    foreach (const QnResourcePtr &res, justfoundList)
    {
        if (res->checkFlags(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;

        QnNetworkResourcePtr nr = res.dynamicCast<QnNetworkResource>();
        if (!nr || !nr->shoudResolveConflicts())
            continue;

        bool inSameSubnet = m_netState.isResourceInMachineSubnet(nr->getHostAddress(), nr->getDiscoveryAddr());

        check_if_accessible_STRUCT t(nr, inSameSubnet );
        checkLst.push_back(t);
    }

#ifdef _DEBUG
    Q_UNUSED(threads);
    foreach(check_if_accessible_STRUCT t, checkLst)
    {
        qDebug() << "Checking conflicts for " << t.resourceNet->getHostAddress().toString() << "  name = " << t.resourceNet->getName();
        t.f();
    }
#else
    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) global->releaseThread();
    QtConcurrent::blockingMap(checkLst, &check_if_accessible_STRUCT::f);
    for (int i = 0; i < threads; ++i )global->reserveThread();
#endif //_DEBUG
}

//====================================================================================

void QnResourceDiscoveryManager::resovle_conflicts(QnResourceList& resourceList, const CLIPList& busy_list, bool *ip_finished)
{
    foreach (const QnResourcePtr &res, resourceList)
    {
        QnNetworkResourcePtr resource = res.dynamicCast<QnNetworkResource>();

        if (!m_netState.existsSubnet(resource->getDiscoveryAddr())) // very strange
            continue;

        CLSubNetState& subnet = m_netState.getSubNetState(resource->getDiscoveryAddr());

        cl_log.log("Looking for next addr...", cl_logALWAYS);

        if (!getNextAvailableAddr(subnet, busy_list))
        {
            *ip_finished = true;			// no more FREE ip left ?
            cl_log.log("No more available IP!!", cl_logERROR);
            break;
        }

        if (resource->setHostAddress(subnet.currHostAddress, QnDomainPhysical) && resource->isResourceAccessible())
        {
            resource->removeNetworkStatus(QnNetworkResource::BadHostAddr);
        }
    }
}
