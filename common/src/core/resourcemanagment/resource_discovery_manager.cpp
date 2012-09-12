#include <QtCore/QtConcurrentMap>
#include <QtCore/QThreadPool>
#include "resource_discovery_manager.h"
#include "utils/common/sleep.h"
#include "resource_searcher.h"
#include "../resource/network_resource.h"
#include "resource_pool.h"
#include "utils/common/util.h"
#include "api/app_server_connection.h"
#include "core/resource/resource_directory_browser.h"
#include "utils/common/synctime.h"
#include "utils/network/ping.h"
#include "utils/network/ip_range_checker.h"


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

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return result;
    if (resourceType->getName() == QLatin1String("Storage"))
    {

        result = QnResourcePtr(QnStoragePluginFactory::instance()->createStorage(parameters[QLatin1String("url")]));
        result->deserialize(parameters);
    }
    else {
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
    }

    return result;
}

void QnResourceDiscoveryManager::pleaseStop()
{
    if (isRunning())
    {
        QMutexLocker locker(&m_searchersListMutex);
        foreach (QnAbstractResourceSearcher *searcher, m_searchersList)
            searcher->pleaseStop();
    }

    QnLongRunnable::pleaseStop();
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

        if (dynamic_cast<QnResourceDirectoryBrowser*>(searcher))
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

    while (!needToStop() && !initCameraHistory(appServerConnection))
    {
        QnSleep::msleep(1000);
    }

    m_runNumber = 0;

    while (!needToStop())
    {
        updateLocalNetworkInterfaces();

        bool ip_finished;
        QnResourceList result = findNewResources(&ip_finished);
        if (ip_finished)
            cl_log.log(QLatin1String("Cannot get available IP address."), cl_logWARNING);

        if (!result.isEmpty())
        {
            m_resourceProcessor->processResources(result);
        }

        int global_delay_between_search = 1000;
        smartSleep(global_delay_between_search);
        ++m_runNumber;
    }
}

void QnResourceDiscoveryManager::updateLocalNetworkInterfaces()
{
    QList<QHostAddress> localAddresses = allLocalAddresses();
    if (localAddresses != m_allLocalAddresses)
    {
        // Skip first time.
        // We suppose here that allLocalAddresses never returns empty list
        if (!m_allLocalAddresses.isEmpty())
            emit localInterfacesChanged();

        m_allLocalAddresses = localAddresses;
    }
}

void printInLogNetResources(const QnResourceList& resources)
{
    foreach(QnResourcePtr res, resources)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (!netRes)
            continue;

        cl_log.log(netRes->getHostAddress().toString() + QLatin1String(" "), netRes->getName(), cl_logINFO);
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
        cl_log.log("looking for resources ===========...", cl_logINFO);

    QnResourceList resources;
    QnResourceList::iterator it;


    ResourceSearcherList searchersList;
    {
        QMutexLocker locker(&m_searchersListMutex);
        searchersList = m_searchersList;
    }

    m_discoveredResources.clear();

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
        if (needToStop())
            return QnResourceList();

        QnResourcePtr rpResource = qnResPool->getResourceByUniqId((*it)->getUniqueId());
        QnNetworkResourcePtr rpNetRes = rpResource.dynamicCast<QnNetworkResource>();

        if (rpResource)
        {
            // if such res in ResourcePool
            QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();

            if (newNetRes)
            {
                if (!newNetRes->hasFlags(QnResource::server_live_cam)) // if this is not camera from mediaserver on the client stand alone
                {
                    quint32 ips = newNetRes->getHostAddress().toIPv4Address();
                    if (ipsList.contains(ips))
                        ipsList[ips]++;
                    else
                        ipsList[ips] = 1;
                }


                
                bool diffAddr = rpNetRes && rpNetRes->getHostAddress() != newNetRes->getHostAddress(); //if such network resource is in pool and has diff IP 
                bool diffNet = !m_netState.isResourceInMachineSubnet(newNetRes->getHostAddress(), newNetRes->getDiscoveryAddr()); // or is diff subnet NET

                // sometimes camera could be found with 2 different nics; sometimes just on one nic. so, diffNet will be detected - but camera is still ok,
                // so status needs to be checked. hasRunningLiveProvider here to avoid situation where there is no recording and live view, but user is about to view the cam. fuck
                bool shouldInvesigate = (diffAddr || diffNet) && ( rpNetRes->getStatus() == QnResource::Offline || !rpNetRes->hasRunningLiveProvider());
                if (shouldInvesigate)
                {
                    // should keep it into resources to investigate it further 
                    ++it;
                    continue;
                }
                else
                {
                    if (rpNetRes->mergeResourcesIfNeeded(newNetRes))
                    {

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
                                    qCritical() << "QnResourceDiscoveryManager::findNewResources(): Can't add camera. Reason: " << errorString;
                                }
                            }

                        }

                    }

                }
            }

            // seems like resource is in the pool and has OK ip
            updateResourceStatus(rpNetRes);
            pingResources(rpNetRes);

            it = resources.erase(it); // do not need to investigate OK resources


        }
        else
            ++it; // new resource => shouls keep it
    }

    //==========if resource is not discovered last minute and we do not record it and do not see live => readers not runing 
    markOfflineIfNeeded();
    //======================

    if (resources.size())
    {
        cl_log.log("Discovery----: after excluding existing resources we've got ", resources.size(), " new resources:", cl_logINFO);
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
        if (res->hasFlags(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;

        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes && netRes->shoudResolveConflicts())
        {
            quint32 ips = netRes->getHostAddress().toIPv4Address();
            if (ipsList.count(ips) > 0 && ipsList[ips] > 1)
            {
                netRes->setNetworkStatus(QnNetworkResource::BadHostAddr);
                cl_log.log(netRes->getHostAddress().toString() , " conflicting. Has same IP as someone else.", cl_logINFO);
            }
        }
    }

    if (resources.size()) {
        m_netState.updateNetState(); // update net state before working with discovered resources
    }

    //marks all new network resources as badip if: 1) not in the same subnet and not accesible or 2) same subnet and conflicting
    cl_log.log("Discovery---- Checking if resources are accessible...", cl_logINFO);
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
                cl_log.log("Ready to go resource: ", netRes->getHostAddress().toString(), cl_logINFO);

            readyToGo.push_back(*it);
            it = resources.erase(it);
        }
    }


    // readyToGo contains ready to go resources
    // resources contains only new network conflicting resources
    // now resources list has only network resources

    cl_log.log("Discovery---- After check_if_accessible readyToGo List: ", cl_logINFO);
    printInLogNetResources(readyToGo);

    cl_log.log("Discovery---- After check_if_accessible conflicting List: ", cl_logINFO);
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


    cl_log.log("Discovery---- After resovle_conflicts - list of non conflicting resource: ", cl_logINFO);
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

    cl_log.log("Discovery---- After requesting additional info from cam... - list of non conflicting resource: ", cl_logINFO);
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
                                qCritical() << "QnResourceDiscoveryManager::findNewResources(): Can't add camera. Reason: " << errorString;
                            }
                        }

                    }

                    continue;
                }
                else
                {
                    if (rpNetRes)
                    {
                        // if we here it means we've got 2 resources with same ip adress.
                        // still could be a different sub_net
                        bool inSameSubnet = m_netState.isResourceInMachineSubnet(rpNetRes->getHostAddress(), rpNetRes->getDiscoveryAddr());
                        if (!inSameSubnet)
                        {
                            it = resources.erase(it);
                            qWarning() << "Found resource " << rpNetRes->getName() << " id=" << rpNetRes->getId().toString() << " in diff subnet but ip address was not changed.";
                            continue;
                        }

                        updateResourceStatus(rpNetRes);
                    }

                    //Q_ASSERT(false);
                    ++it;
                    continue;
                }
            }

            ++it;
            continue;

        }
        else
            ++it; // nothing to do
    }

    if (resources.size())
    {
        cl_log.log("Discovery---- Final result: ", cl_logINFO);
        printInLogNetResources(resources);
    }
    

    return resources;
}

struct ManualSearcherHelper
{
    QHostAddress addrToCheck;
    QnResourceDiscoveryManager::ResourceSearcherList* plugins;
    QnResourcePtr result;
    QAuthenticator auth;

    void f()
    {
        foreach(QnAbstractResourceSearcher* as, *plugins)
        {
            QnAbstractNetworkResourceSearcher* ns = dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
            result = ns->checkHostAddr(addrToCheck, auth);
            if (result) {
                break;
            }
        }
    }
};


QnResourceList QnResourceDiscoveryManager::findResources(QHostAddress startAddr, QHostAddress endAddr, const QAuthenticator& auth)
{
    
    {
        QString str;
        QTextStream stream(&str);

        stream << "Looking for cameras... StartAddr = " << startAddr.toString() << "  EndAddr = " << endAddr.toString() << "   login/pass = " << auth.user() << "/" << auth.password();
        cl_log.log(str, cl_logINFO);
    }
    
    //=======================================
    QnIprangeChecker ip_cheker;

    cl_log.log("Checking for online addresses....", cl_logINFO);

    QList<QHostAddress> online = ip_cheker.onlineHosts(startAddr, endAddr);


    cl_log.log("Found ", online.size(), " IPs:", cl_logINFO);

    foreach(QHostAddress addr, online)
    {
        cl_log.log(addr.toString(), cl_logINFO);
    }

    //=======================================

    //now lets check each one of online machines...

    QList<ManualSearcherHelper> testList;
    foreach(QHostAddress addr, online)
    {
        ManualSearcherHelper t;
        t.addrToCheck = addr;
        t.auth = auth;
        t.plugins = &m_searchersList; // I assume m_searchersList is constatnt during server life cycle 
        testList.push_back(t);
    }

    /*
    int threads = 4;
    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) 
        global->releaseThread();
    QtConcurrent::blockingMap(testList, &ManualSearcherHelper::f);
    for (int i = 0; i < threads; ++i )
        global->reserveThread();
    */
    int startIdx = 0;
    static const int SEARCH_THREAD_AMOUNT = 4;
    int endIdx = qMin(testList.size(), startIdx + SEARCH_THREAD_AMOUNT);
    while (startIdx < testList.size()) {
        QtConcurrent::blockingMap(testList.begin() + startIdx, testList.begin() + endIdx, &ManualSearcherHelper::f);
        startIdx = endIdx;
        endIdx = qMin(testList.size(), startIdx + SEARCH_THREAD_AMOUNT);
    }


    QnResourceList result;

    foreach(const ManualSearcherHelper& h, testList)
    {
        if (!h.result)
            continue;

        if (qnResPool->hasSuchResouce(h.result->getUniqueId())) // already in resource pool 
            continue;

        result.push_back(h.result);

        //qnResPool->

    }

    cl_log.log("Found ",  result.size(), " new resources", cl_logINFO);
    foreach(QnResourcePtr res, result)
    {
        cl_log.log(res->toString(), cl_logINFO);
    }


    return result;
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
                cl_log.log(resourceNet->getHostAddress().toString() + QLatin1String("  name = ") +  resourceNet->getName(), " has bad IP(same subnet)", cl_logWARNING);
            else
                cl_log.log(resourceNet->getHostAddress().toString() + QLatin1String("  name = ") +  resourceNet->getName(), " has bad IP(diff subnet)", cl_logWARNING);
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

        if (res->hasFlags(QnResource::server_live_cam)) // if this is camera from mediaserver on the client
            continue;

        if (res->isDisabled()) // if this is camera from other mediaserver 
            continue;

        QDateTime ldt = netRes->getLastDiscoveredTime();
        QDateTime currentT = qnSyncTime->currentDateTime();

        //check if resource was discovered
        QString uniqId = res->getUniqueId();
        if (!m_discoveredResources.contains(uniqId))
        {
            // resource is not found
            m_resourceDiscoveryCounter[uniqId]++;

            if (m_resourceDiscoveryCounter[uniqId] >= 5 && !netRes->hasRunningLiveProvider())
            {
                res->setStatus(QnResource::Offline);
                m_resourceDiscoveryCounter[uniqId] = 0;
            }
        }
        else {
            m_resourceDiscoveryCounter[uniqId] = 0;
        }
     

        /*
        if (ldt.secsTo(currentT) > 120 && !netRes->hasRunningLiveProvider()) // if resource is not discovered last 30 sec
        {
            res->setStatus(QnResource::Offline);
        }
        */

    }

    
}

void QnResourceDiscoveryManager::updateResourceStatus(QnResourcePtr res)
{
    // seems like resource is in the pool and has OK ip
    QnNetworkResourcePtr rpNetRes = res.dynamicCast<QnNetworkResource>();

    if (rpNetRes)
    {
        disconnect(rpNetRes.data(), SIGNAL(initAsyncFinished(QnResourcePtr, bool)), this, SLOT(onInitAsyncFinished(QnResourcePtr, bool)));
        connect(rpNetRes.data(), SIGNAL(initAsyncFinished(QnResourcePtr, bool)), this, SLOT(onInitAsyncFinished(QnResourcePtr, bool)));

        if (rpNetRes->getStatus() == QnResource::Offline) 
        {
            if (rpNetRes->getLastStatusUpdateTime().msecsTo(qnSyncTime->currentDateTime()) > 30) // if resource with OK ip seems to be found; I do it coz if there is no readers and camera was offline and now online => status needs to be changed
            {
                rpNetRes->initAsync();
                //rpNetRes->setStatus(QnResource::Online);
            }

        }
        else if (!rpNetRes->isInitialized())
        {
            rpNetRes->initAsync(); // Resource already in resource pool. Try to init resource if resource is not authorized or not initialized by other reason
            //if (rpNetRes->isInitialized() && rpNetRes->getStatus() == QnResource::Unauthorized)
            //    rpNetRes->setStatus(QnResource::Online);
        }

        m_discoveredResources.insert(rpNetRes->getUniqueId());
        rpNetRes->setLastDiscoveredTime(qnSyncTime->currentDateTime());
    }

}

void QnResourceDiscoveryManager::onInitAsyncFinished(QnResourcePtr res, bool initialized)
{
    QnNetworkResourcePtr rpNetRes = res.dynamicCast<QnNetworkResource>();
    if (initialized && rpNetRes)
    {
        if (rpNetRes->getStatus() == QnResource::Offline || rpNetRes->getStatus() == QnResource::Unauthorized)
            rpNetRes->setStatus(QnResource::Online);
    }
}

void QnResourceDiscoveryManager::pingResources(QnResourcePtr res)
{
    if (m_runNumber%50 != 0)
        return;

    QnNetworkResourcePtr rpNetRes = res.dynamicCast<QnNetworkResource>();

    if (rpNetRes)
    {
        if (!rpNetRes->hasRunningLiveProvider())
        {
            CLPing ping;
            ping.ping(rpNetRes->getHostAddress().toString(), 1, 300);
        }
    }
}

void QnResourceDiscoveryManager::check_if_accessible(QnResourceList& justfoundList, int threads)
{
    QList<check_if_accessible_STRUCT> checkLst;

    foreach (const QnResourcePtr &res, justfoundList)
    {
        if (res->hasFlags(QnResource::server_live_cam)) // if this is camera from mediaserver
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

        cl_log.log("Looking for next addr...", cl_logINFO);

        if (!getNextAvailableAddr(subnet, busy_list))
        {
            *ip_finished = true;            // no more FREE ip left ?
            cl_log.log("No more available IP!!", cl_logERROR);
            break;
        }

        if (resource->setHostAddress(subnet.currHostAddress, QnDomainPhysical) && resource->isResourceAccessible())
        {
            resource->removeNetworkStatus(QnNetworkResource::BadHostAddr);
        }
    }
}
