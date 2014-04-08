#include "mserver_resource_discovery_manager.h"

#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/QThreadPool>

#include "api/app_server_connection.h"

#include "business/business_event_connector.h"

#include "core/dataprovider/live_stream_provider.h"
#include <core/resource/camera_resource.h>
#include "core/resource/network_resource.h"
#include "core/resource/abstract_storage_resource.h"
#include "core/resource/storage_resource.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource_management/resource_searcher.h"

#include "plugins/storage/dts/abstract_dts_searcher.h"

#include "utils/common/synctime.h"
#include "utils/network/ping.h"
#include "utils/network/ip_range_checker.h"
#include "utils/common/sleep.h"
#include "utils/common/util.h"

static const int NETSTATE_UPDATE_TIME = 1000 * 30;

QnMServerResourceDiscoveryManager::QnMServerResourceDiscoveryManager( const CameraDriverRestrictionList& cameraDriverRestrictionList )
:
    QnResourceDiscoveryManager( &cameraDriverRestrictionList ),
    m_foundSmth(false)
{
    netStateTime.restart();
    connect(this, SIGNAL(cameraDisconnected(QnResourcePtr, qint64)), qnBusinessRuleConnector, SLOT(at_cameraDisconnected(const QnResourcePtr&, qint64)));

}

QnMServerResourceDiscoveryManager::~QnMServerResourceDiscoveryManager()
{
    stop();
}

void printInLogNetResources(const QnResourceList& resources)
{
    foreach(QnResourcePtr res, resources)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (!netRes)
            continue;

        cl_log.log(netRes->getHostAddress() + QLatin1String(" "), netRes->getName(), cl_logINFO);
    }

}


bool QnMServerResourceDiscoveryManager::processDiscoveredResources(QnResourceList& resources)
{
    QMutexLocker lock(&m_discoveryMutex);

    if (netStateTime.elapsed() > NETSTATE_UPDATE_TIME) {
        netState.updateNetState();
        netStateTime.restart();
    }


    QSet<QString> discoveredResources;

    //assemble list of existing ip
    QMap<quint32, QSet<QnNetworkResourcePtr> > ipsList;


    //excluding already existing resources
    QnResourceList::iterator it = resources.begin();
    while (it != resources.end())
    {
        if (needToStop())
            return false;

        QnResourcePtr rpResource = qnResPool->getResourceByUniqId((*it)->getUniqueId());
        QnNetworkResourcePtr rpNetRes = rpResource.dynamicCast<QnNetworkResource>();

        if (rpResource && !rpResource->hasFlags(QnResource::foreigner))
        {
            // if such res in ResourcePool
            QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();

            if (newNetRes)
            {
                if (!newNetRes->hasFlags(QnResource::server_live_cam)) // if this is not camera from mediaserver on the client stand alone
                {
                    QnSecurityCamResourcePtr camRes = newNetRes.dynamicCast<QnSecurityCamResource>();
                    if (camRes && camRes->needCheckIpConflicts())
                    {
                        // do not count 2--N channels of multichannel cameras as conflict
                        quint32 ips = resolveAddress(newNetRes->getHostAddress()).toIPv4Address();
                        if (ips)
                            ipsList[ips].insert(newNetRes);
                    }
                }



                bool diffAddr = rpNetRes && rpNetRes->getHostAddress() != newNetRes->getHostAddress(); //if such network resource is in pool and has diff IP 
                bool diffNet = false;
                if (newNetRes->getDiscoveryAddr().toIPv4Address() != 0)
                    diffNet = !netState.isResourceInMachineSubnet(newNetRes->getHostAddress(), newNetRes->getDiscoveryAddr()); // or is diff subnet NET

                // sometimes camera could be found with 2 different nics; sometimes just on one nic. so, diffNet will be detected - but camera is still ok,
                // so status needs to be checked. hasRunningLiveProvider here to avoid situation where there is no recording and live view, but user is about to view the cam. fuck
                bool shouldInvesigate = (diffAddr || (diffNet && newNetRes->shoudResolveConflicts() )) && 
                                        ( rpNetRes->getStatus() == QnResource::Offline || !QnLiveStreamProvider::hasRunningLiveProvider(rpNetRes));
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

                        // not stand alone
                        QnVirtualCameraResourcePtr cameraResource = rpNetRes.dynamicCast<QnVirtualCameraResource>();
                        if (cameraResource)
                        {
                            QByteArray errorString;
                            QnVirtualCameraResourceList cameras;
                            ec2::AbstractECConnectionPtr connect = QnAppServerConnectionFactory::getConnection2();
                            const ec2::ErrorCode errorCode = connect->getCameraManager()->addCameraSync( cameraResource, &cameras );
                            if( errorCode != ec2::ErrorCode::ok )
                                NX_LOG( QString::fromLatin1("Can't add camera to ec2. %1").arg(ec2::toString(errorCode)), cl_logWARNING );
                        }

                    }

                }
            }

            // seems like resource is in the pool and has OK ip
            updateResourceStatus(rpNetRes, discoveredResources);
            pingResources(rpNetRes);

            it = resources.erase(it); // do not need to investigate OK resources


        }
        else 
        {
            ++it; // new resource => shouls keep it
        }
    }

    // ========================= send conflict info =====================
    for (QMap<quint32, QSet<QnNetworkResourcePtr> >::iterator itr = ipsList.begin(); itr != ipsList.end(); ++itr)
    {
        if (itr.value().size() > 1) 
        {
            QHostAddress hostAddr(itr.key());
            QStringList conflicts;
            foreach(QnNetworkResourcePtr camRes, itr.value()) 
            {
                conflicts << camRes->getPhysicalId();
                QnVirtualCameraResourcePtr cam = camRes.dynamicCast<QnVirtualCameraResource>();
                if (cam)
                    cam->issueOccured();
            }
            emit CameraIPConflict(hostAddr, conflicts);
        }
    }

    //==========if resource is not discovered last minute and we do not record it and do not see live => readers not runing 
    markOfflineIfNeeded(discoveredResources);
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
        return true;


    // now let's mark conflicting resources( just new )
    // in pool could not be 2 resources with same ip
    foreach (const QnResourcePtr &res, resources)
    {
        if (res->hasFlags(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;

        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes && netRes->shoudResolveConflicts())
        {
            quint32 ips = resolveAddress(netRes->getHostAddress()).toIPv4Address();
            if (ipsList.count(ips) > 0 && ipsList[ips].size() > 1)
            {
                netRes->setNetworkStatus(QnNetworkResource::BadHostAddr);
                cl_log.log(netRes->getHostAddress() , " conflicting. Has same IP as someone else.", cl_logINFO);
            }
        }
    }

    //marks all new network resources as badip if: 1) not in the same subnet and not accesible or 2) same subnet and conflicting
    cl_log.log("Discovery---- Checking if resources are accessible...", cl_logINFO);
    static const int  threads = 5;
    check_if_accessible(resources, threads, netState);


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
                cl_log.log("Ready to go resource: ", netRes->getHostAddress(), cl_logINFO);

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

    if (resources.size())
        cl_log.log("Discovery---- Changing IP addresses... ", cl_logDEBUG1);

    bool ip_finished = false;
    resovle_conflicts(resources, ipsList.keys(), &ip_finished, netState);

    if (ip_finished)
        cl_log.log(QLatin1String("Cannot get available IP address."), cl_logWARNING);



    // lets remove still not accessible resources
    it = resources.begin();
    while (it != resources.end())
    {
        QnNetworkResourcePtr netRes = (*it).dynamicCast<QnNetworkResource>();
        if (netRes && netRes->checkNetworkStatus(QnNetworkResource::BadHostAddr)) // if this is network resource and it has bad ip should stay
        {
            it = resources.erase(it);
            cl_log.log("!!! Cannot resolve conflict for: ", netRes->getHostAddress(), cl_logERROR);
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

                    // not stand alone
                    QnVirtualCameraResourcePtr cameraResource = rpNetRes.dynamicCast<QnVirtualCameraResource>();
                    if (cameraResource)
                    {
                        QnVirtualCameraResourceList cameras;
                        ec2::AbstractECConnectionPtr connect = QnAppServerConnectionFactory::getConnection2();
                        const ec2::ErrorCode errorCode = connect->getCameraManager()->addCameraSync( cameraResource, &cameras );
                        if( errorCode != ec2::ErrorCode::ok )
                            NX_LOG( QString::fromLatin1("Can't add camera to ec2. %1").arg(ec2::toString(errorCode)), cl_logWARNING );
                    }

                    continue;
                }
                else
                {
                    if (rpNetRes)
                    {
                        // if we here it means we've got 2 resources with same ip adress.
                        // still could be a different sub_net
                        bool inSameSubnet = true;
                        if (rpNetRes->getDiscoveryAddr().toIPv4Address() != 0)
                            inSameSubnet = netState.isResourceInMachineSubnet(rpNetRes->getHostAddress(), rpNetRes->getDiscoveryAddr());
                        if (!inSameSubnet)
                        {
                            it = resources.erase(it);
                            qWarning() << "Found resource " << rpNetRes->getName() << " id=" << rpNetRes->getId().toString() << " in diff subnet but ip address was not changed.";
                            continue;
                        }

                        updateResourceStatus(rpNetRes, discoveredResources);
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
    return true;
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
                cl_log.log(resourceNet->getHostAddress() + QLatin1String("  name = ") +  resourceNet->getName(), " has bad IP(same subnet)", cl_logWARNING);
            else
                cl_log.log(resourceNet->getHostAddress() + QLatin1String("  name = ") +  resourceNet->getName(), " has bad IP(diff subnet)", cl_logWARNING);
        }

    }
};


void QnMServerResourceDiscoveryManager::markOfflineIfNeeded(QSet<QString>& discoveredResources)
{
    QnResourceList resources = qnResPool->getResources();

    foreach(QnResourcePtr res, resources)
    {
        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (!netRes)
            continue;

        if (res->hasFlags(QnResource::server_live_cam)) // if this is camera from mediaserver on the client
            continue;

        if( res->hasFlags(QnResource::foreigner) )      //this camera belongs to some other mediaserver
        {
            continue;
        }

        QDateTime ldt = netRes->getLastDiscoveredTime();
        QDateTime currentT = qnSyncTime->currentDateTime();

        //check if resource was discovered
        QString uniqId = res->getUniqueId();
        if (!discoveredResources.contains(uniqId))
        {
            // resource is not found
            m_resourceDiscoveryCounter[uniqId]++;


            if (m_resourceDiscoveryCounter[uniqId] >= 5)
            {
                QnVirtualCameraResourcePtr camRes = netRes.dynamicCast<QnVirtualCameraResource>();
                if (QnLiveStreamProvider::hasRunningLiveProvider(netRes)  || (camRes && !camRes->isScheduleDisabled())) {
                    if (res->getStatus() == QnResource::Offline && !m_disconnectSended[uniqId]) {
                        QnVirtualCameraResourcePtr cam = res.dynamicCast<QnVirtualCameraResource>();
                        if (cam)
                            cam->issueOccured();
                        emit cameraDisconnected(res, qnSyncTime->currentUSecsSinceEpoch());
                        m_disconnectSended[uniqId] = true;
                    }
                } else {
                    res->setStatus(QnResource::Offline);
                    m_resourceDiscoveryCounter[uniqId] = 0;
                }
            }
        }
        else {
            m_resourceDiscoveryCounter[uniqId] = 0;
            m_disconnectSended[uniqId] = false;
        }
     

        /*
        if (ldt.secsTo(currentT) > 120 && !netRes->hasRunningLiveProvider()) // if resource is not discovered last 30 sec
        {
            res->setStatus(QnResource::Offline);
        }
        */

    }

    
}

void QnMServerResourceDiscoveryManager::updateResourceStatus(QnResourcePtr res, QSet<QString>& discoveredResources)
{
    // seems like resource is in the pool and has OK ip
    QnNetworkResourcePtr rpNetRes = res.dynamicCast<QnNetworkResource>();

    if (rpNetRes)
    {
        disconnect(rpNetRes.data(), SIGNAL(initAsyncFinished(QnResourcePtr, bool)), this, SLOT(onInitAsyncFinished(QnResourcePtr, bool)));
        connect(rpNetRes.data(), SIGNAL(initAsyncFinished(QnResourcePtr, bool)), this, SLOT(onInitAsyncFinished(QnResourcePtr, bool)));

        if (!rpNetRes->hasFlags(QnResource::foreigner))
        {
            if (rpNetRes->getStatus() == QnResource::Offline) 
            {
                if (rpNetRes->getLastStatusUpdateTime().msecsTo(qnSyncTime->currentDateTime()) > 30) // if resource with OK ip seems to be found; I do it coz if there is no readers and camera was offline and now online => status needs to be changed
                {
                    rpNetRes->initAsync(false);
                    //rpNetRes->setStatus(QnResource::Online);
                }

            }
            else if (!rpNetRes->isInitialized())
            {
                rpNetRes->initAsync(false); // Resource already in resource pool. Try to init resource if resource is not authorized or not initialized by other reason
                //if (rpNetRes->isInitialized() && rpNetRes->getStatus() == QnResource::Unauthorized)
                //    rpNetRes->setStatus(QnResource::Online);
            }
        }
        discoveredResources.insert(rpNetRes->getUniqueId());
        rpNetRes->setLastDiscoveredTime(qnSyncTime->currentDateTime());
    }

}

//====================================================================================

void QnMServerResourceDiscoveryManager::resovle_conflicts(QnResourceList& resourceList, const CLIPList& busy_list, bool *ip_finished, CLNetState& netState)
{
    foreach (const QnResourcePtr &res, resourceList)
    {
        QnNetworkResourcePtr resource = res.dynamicCast<QnNetworkResource>();

        if (resource->getDiscoveryAddr().toIPv4Address() == 0 || !netState.existsSubnet(resource->getDiscoveryAddr())) // very strange
            continue;

        CLSubNetState& subnet = netState.getSubNetState(resource->getDiscoveryAddr());

        cl_log.log("Looking for next addr...", cl_logINFO);

        if (!getNextAvailableAddr(subnet, busy_list))
        {
            *ip_finished = true;            // no more FREE ip left ?
            cl_log.log("No more available IP!!", cl_logERROR);
            break;
        }

        if (resource->setHostAddress(subnet.currHostAddress.toString(), QnDomainPhysical) && resource->isResourceAccessible())
        {
            resource->removeNetworkStatus(QnNetworkResource::BadHostAddr);
        }
    }
}

void QnMServerResourceDiscoveryManager::pingResources(QnResourcePtr res)
{
    if (m_runNumber%50 != 0)
        return;

    QnNetworkResourcePtr rpNetRes = res.dynamicCast<QnNetworkResource>();

    if (rpNetRes)
    {
        if (!QnLiveStreamProvider::hasRunningLiveProvider(rpNetRes))
        {
            CLPing ping;
            ping.ping(rpNetRes->getHostAddress(), 1, 300);
        }
    }
}

void QnMServerResourceDiscoveryManager::check_if_accessible(QnResourceList& justfoundList, int threads, CLNetState& netState)
{
    QList<check_if_accessible_STRUCT> checkLst;

    foreach (const QnResourcePtr &res, justfoundList)
    {
        if (res->hasFlags(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;

        QnNetworkResourcePtr nr = res.dynamicCast<QnNetworkResource>();
        if (!nr || !nr->shoudResolveConflicts())
            continue;

        bool inSameSubnet = nr->getDiscoveryAddr().toIPv4Address() == 0 || netState.isResourceInMachineSubnet(nr->getHostAddress(), nr->getDiscoveryAddr());

        check_if_accessible_STRUCT t(nr, inSameSubnet );
        checkLst.push_back(t);
    }

#ifdef _DEBUG
    Q_UNUSED(threads);
    foreach(check_if_accessible_STRUCT t, checkLst)
    {
        NX_LOG(QString(lit("Checking conflicts for %1 name = %2")).arg(t.resourceNet->getHostAddress()).arg(t.resourceNet->getName()), cl_logDEBUG1);

        t.f();
    }
#else
    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) global->releaseThread();
    QtConcurrent::blockingMap(checkLst, &check_if_accessible_STRUCT::f);
    for (int i = 0; i < threads; ++i )global->reserveThread();
#endif //_DEBUG
}
