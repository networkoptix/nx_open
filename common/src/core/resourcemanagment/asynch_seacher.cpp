#include <QtCore/QtConcurrentMap>
#include <QtCore/QThreadPool>
#include "asynch_seacher.h"
#include "utils/common/sleep.h"
#include "resourceserver.h"
#include "../resource/network_resource.h"
#include "resource_pool.h"
#include "utils/common/util.h"
#include "api/AppServerConnection.h"

QnResourceDiscoveryManager::QnResourceDiscoveryManager():
m_server(false)
{
}

QnResourceDiscoveryManager::~QnResourceDiscoveryManager()
{
    stop();
}

QnResourceDiscoveryManager& QnResourceDiscoveryManager::instance()
{
    static QnResourceDiscoveryManager instance;

    return instance;
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

QnResourcePtr QnResourceDiscoveryManager::createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters)
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
        searcher->setShouldBeUsed(true);

    QnAppServerConnectionPtr appServerConnection = QnAppServerConnectionFactory::createConnection();
    while (!needToStop() && !initResourceTypes(appServerConnection))
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

            foreach (const QnResourcePtr &res, result) 
            {
                QnResourcePtr resource = qnResPool->getResourceByUniqId(res->getUniqueId());
                if (!resource.isNull() && !resource.dynamicCast<QnCameraResource>()) // if this is not client and cams did not came from server
                    resource->setStatus(QnResource::Online);
            }


        }

        int global_delay_between_search = 1000;
        smartSleep(global_delay_between_search);
    }
}

QnResourceList QnResourceDiscoveryManager::findNewResources(bool *ip_finished)
{
    //bool allow_to_change_ip = true;
    static const int  threads = 5;

    *ip_finished = false;

    QTime time;
    time.start();

    //====================================
    //CL_LOG(cl_logDEBUG1) cl_log.log("looking for resources...", cl_logDEBUG1);

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
            QnResourceList lst = searcher->findResources();
            foreach (const QnResourcePtr &res, lst)
                res->addFlag(searcher->isLocal() ? QnResource::local : QnResource::remote);

            resources.append(lst);
        }
    }

    //excluding already existing resources
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
                    ++it;
                    continue;
                }
            }


            it = resources.erase(it);
        }
        else
            ++it; // new resource => shouls keep it
    }

    //qDebug() << resources.size();
    //assemble list of existing ip

    // from pool
    QMap<quint32, int> ipsList;
    foreach (const QnResourcePtr &res, qnResPool->getResourcesWithFlag(QnResource::network))
    {
        if (res->checkFlag(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;

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
    foreach (const QnResourcePtr &res, resources)
    {
        //if (qnResPool->hasSuchResouce(res->getUniqueId()))
        //    continue; // this ip is already taken into account

        if (res->checkFlag(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;


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
    foreach (const QnResourcePtr &res, resources)
    {
        if (res->checkFlag(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;

        QnNetworkResourcePtr netRes = res.dynamicCast<QnNetworkResource>();
        if (netRes)
        {
            quint32 ips = netRes->getHostAddress().toIPv4Address();
            if (ipsList.count(ips) > 0 && ipsList[ips] > 1)
                netRes->setNetworkStatus(QnNetworkResource::BadHostAddr);
        }
    }

    if (resources.size()) {
        m_netState.updateNetState(); // update net state before working with discovered resources
    }

    //marks all new network resources as badip if: 1) not in the same subnet and not accesible or 2) same subnet and conflicting
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
            readyToGo.push_back(*it);
            it = resources.erase(it);
        }
    }


    // readyToGo contains ready to go resources
    // resources contains only new network conflicting resources

    // now resources list has only network resources
    if (resources.size()) {
        cl_log.log("Found ", resources.size(), " conflicting resources.", cl_logDEBUG1);
        cl_log.log("Time elapsed: ", time.elapsed(), cl_logDEBUG1);
    }
    //======================================

    time.restart();
    if (resources.size())
        cl_log.log("Changing IP addresses... ", cl_logDEBUG1);

    resovle_conflicts(resources, ipsList.keys(), ip_finished);

    if (resources.size())
        cl_log.log("Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);



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
    if (resources.size())
        qDebug() << "Returning " << resources.size() << " resources";


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
                if (rpNetRes)
                {
                    // if such network resource is in pool and has diff IP => should keep it
                    rpNetRes->setHostAddress(newNetRes->getHostAddress(), QnDomainMemory);
                    it = resources.erase(it);

                    if (isServer())
                    {
                        // not stand alone 
                        QnCameraResourcePtr cameraResource = rpNetRes.dynamicCast<QnCameraResource>();
                        if (cameraResource)
                        {
                            QByteArray errorString;
                            QnResourceList cameras;
                            QnAppServerConnectionPtr connect = QnAppServerConnectionFactory::createConnection();
                            if (connect->addCamera(*cameraResource, cameras, errorString) != 0)
                            {
                                qDebug() << "QnResourceDiscoveryManager::findNewResources(): Can't add camera. Reason: " << errorString;
                            }
                        }

                    }

                    continue;
                }
                else
                {
                    Q_ASSERT(false);
                }
            }

            Q_ASSERT(false);


        }
        else
            ++it; // nothing to do
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
            resourceNet->addNetworkStatus(QnNetworkResource::BadHostAddr);

    }
};

void QnResourceDiscoveryManager::check_if_accessible(QnResourceList& justfoundList, int threads)
{
    QList<check_if_accessible_STRUCT> checkLst;

    foreach (const QnResourcePtr &res, justfoundList)
    {
        if (res->checkFlag(QnResource::server_live_cam)) // if this is camera from mediaserver
            continue;

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

void QnResourceDiscoveryManager::resovle_conflicts(QnResourceList& resourceList, const CLIPList& busy_list, bool *ip_finished)
{
    foreach (const QnResourcePtr &res, resourceList)
    {
        QnNetworkResourcePtr resource = res.dynamicCast<QnNetworkResource>();

        if (!m_netState.existsSubnet(resource->getDiscoveryAddr())) // very strange
            continue;

        CLSubNetState& subnet = m_netState.getSubNetState(resource->getDiscoveryAddr());

        cl_log.log("Looking for next addr...", cl_logDEBUG1);

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
