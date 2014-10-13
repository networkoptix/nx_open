#include "mserver_resource_discovery_manager.h"

#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/QThreadPool>

#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <utils/common/log.h>
#include <utils/network/ping.h>
#include <utils/network/ip_range_checker.h>

#include "api/app_server_connection.h"
#include "business/business_event_connector.h"
#include "core/dataprovider/live_stream_provider.h"
#include <core/resource/camera_resource.h>
#include "core/resource/network_resource.h"
#include "core/resource/abstract_storage_resource.h"
#include "core/resource/storage_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource_management/resource_searcher.h"
#include "plugins/storage/dts/abstract_dts_searcher.h"
#include "common/common_module.h"
#include "data_only_camera_resource.h"
#include "media_server/settings.h"

static const int NETSTATE_UPDATE_TIME = 1000 * 30;

QnMServerResourceDiscoveryManager::QnMServerResourceDiscoveryManager()
:
    m_foundSmth(false)
{
    netStateTime.restart();
    connect(this, &QnMServerResourceDiscoveryManager::cameraDisconnected, qnBusinessRuleConnector, &QnBusinessEventConnector::at_cameraDisconnected);
    m_serverOfflineTimeout = MSSettings::roSettings()->value("redundancyTimeout", 20).toInt() * 1000;
    m_serverOfflineTimeout = qMax(1000, m_serverOfflineTimeout);
}

QnMServerResourceDiscoveryManager::~QnMServerResourceDiscoveryManager()
{
    stop();
}

QnResourcePtr QnMServerResourceDiscoveryManager::createResource(const QnUuid &resourceTypeId, const QnResourceParams &params)
{
    QnResourcePtr res = QnResourceDiscoveryManager::createResource( resourceTypeId, params );
    if( res )
        return res;

    const QnResourceTypePtr& resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if( !resourceType )
        return res;
    return QnResourcePtr(new DataOnlyCameraResource( resourceTypeId ));   //found resource type, but could not find factory. Disabled discovery?
}

static void printInLogNetResources(const QnResourceList& resources)
{
    foreach(QnResourcePtr res, resources)
    {
        const QnNetworkResource* netRes = dynamic_cast<const QnNetworkResource*>(res.data());
        if (!netRes)
            continue;

        NX_LOG( lit("%1 %2").arg(netRes->getHostAddress()).arg(netRes->getName()), cl_logINFO);
    }

}

bool QnMServerResourceDiscoveryManager::canTakeForeignCamera(const QnSecurityCamResourcePtr& camera, int awaitingToMoveCameraCnt)
{
    if (!camera)
        return false;

    QnUuid ownGuid = qnCommon->moduleGUID();
    QnMediaServerResourcePtr mServer = qnResPool->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();
    QnMediaServerResourcePtr ownServer = qnResPool->getResourceById(ownGuid).dynamicCast<QnMediaServerResource>();
    if (!mServer || !ownServer)
        return false;

#ifdef EDGE_SERVER
    if (!ownServer->isRedundancy()) 
    {
        // return own camera back for edge server
        char  mac[MAC_ADDR_LEN];
        char* host = 0;
        getMacFromPrimaryIF(mac, &host);
        return (camera->getUniqueId().toLocal8Bit() == QByteArray(mac));
    }
#endif
    if ((mServer->getServerFlags() & Qn::SF_Edge) && !mServer->isRedundancy())
        return false; // do not transfer cameras from edge server

    if (camera->preferedServerId() == ownGuid)
        return true;
    else if (mServer->getStatus() == Qn::Online)
        return false;

    if (!ownServer->isRedundancy())
        return false; // redundancy is disabled

    if (qnResPool->getAllCameras(ownServer, true).size() + awaitingToMoveCameraCnt >= ownServer->getMaxCameras())
        return false;
    
    return mServer->currentStatusTime() > m_serverOfflineTimeout;
}

bool QnMServerResourceDiscoveryManager::processDiscoveredResources(QnResourceList& resources)
{
    QMutexLocker lock(&m_discoveryMutex);

    QnResourceList extraResources;

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

        QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();
        if (!newNetRes) {
            it = resources.erase(it); // do not need to investigate OK resources
            continue;
        }

        QnResourcePtr rpResource = qnResPool->getResourceByUniqId(newNetRes->getUniqueId());
        if (!rpResource) {
            ++it; // keep new resource in a list
            continue;
        }

        QnNetworkResourcePtr rpNetRes = rpResource.dynamicCast<QnNetworkResource>();

        // if such res in ResourcePool

        if (rpResource->hasFlags(Qn::foreigner))
        {
            if (!canTakeForeignCamera(rpResource.dynamicCast<QnSecurityCamResource>(), extraResources.size())) 
            {
                it = resources.erase(it); // do not touch foreign resource
                continue;
            }
        }

        QnVirtualCameraResourcePtr newCamRes = newNetRes.dynamicCast<QnVirtualCameraResource>();
        if (newCamRes && newCamRes->needCheckIpConflicts())
        {
            // do not count 2--N channels of multichannel cameras as conflict
            quint32 ips = resolveAddress(newNetRes->getHostAddress()).toIPv4Address();
            if (ips)
                ipsList[ips].insert(newNetRes);
        }

        bool isForeign = rpResource->hasFlags(Qn::foreigner);
        QnVirtualCameraResourcePtr existCamRes = rpNetRes.dynamicCast<QnVirtualCameraResource>();
        if (existCamRes)
        {
            QnUuid newTypeId = newNetRes->getTypeId();
            bool updateTypeId = existCamRes->getTypeId() != newNetRes->getTypeId() && !newNetRes->isAbstractResource();
            if (rpNetRes->mergeResourcesIfNeeded(newNetRes) || isForeign || updateTypeId)
            {
                if (isForeign || updateTypeId) {
                    newNetRes->update(existCamRes);
                    newNetRes->setParentId(qnCommon->moduleGUID());
                    newNetRes->setFlags(existCamRes->flags() & ~Qn::foreigner);
                    newNetRes->setId(existCamRes->getId());
                    newNetRes->addFlags(Qn::parent_change);
                    if (updateTypeId)
                        newNetRes->setTypeId(newTypeId);
                    extraResources << newNetRes;
                }
                else {
                    QByteArray errorString;
                    QnVirtualCameraResourceList cameras;
                    ec2::AbstractECConnectionPtr connect = QnAppServerConnectionFactory::getConnection2();
                    const ec2::ErrorCode errorCode = connect->getCameraManager()->addCameraSync( existCamRes, &cameras );
                    if( errorCode != ec2::ErrorCode::ok )
                        NX_LOG( QString::fromLatin1("Can't add camera to ec2. %1").arg(ec2::toString(errorCode)), cl_logWARNING );
                }
            }
        }

        // seems like resource is in the pool and has OK ip
        updateResourceStatus(rpNetRes, discoveredResources);
        pingResources(rpNetRes);

        it = resources.erase(it); // do not need to investigate OK resources
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
                QnVirtualCameraResource* cam = dynamic_cast<QnVirtualCameraResource*>(camRes.data());
                if (cam)
                    cam->issueOccured();
            }
            emit CameraIPConflict(hostAddr, conflicts);
        }
    }

    //==========if resource is not discovered last minute and we do not record it and do not see live => readers not runing 
    markOfflineIfNeeded(discoveredResources);
    //======================

    m_foundSmth = !resources.isEmpty();
    if (m_foundSmth)
        NX_LOG( lit("Discovery----: after excluding existing resources we've got %1 new resources:").arg(resources.size()), cl_logINFO);

    printInLogNetResources(resources);

    if (resources.isEmpty())
    {
        resources << extraResources;
        return true;
    }



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

    resources << extraResources;

    if (resources.size())
    {
        NX_LOG("Discovery---- Final result: ", cl_logINFO);
        printInLogNetResources(resources);
    }
    return true;
}

void QnMServerResourceDiscoveryManager::markOfflineIfNeeded(QSet<QString>& discoveredResources)
{
    const QnResourceList& resources = qnResPool->getResources();

    foreach(QnResourcePtr res, resources)
    {
        QnNetworkResource* netRes = dynamic_cast<QnNetworkResource*>(res.data());
        if (!netRes)
            continue;

        if (res->hasFlags(Qn::server_live_cam)) // if this is camera from mediaserver on the client
            continue;

        if( res->hasFlags(Qn::foreigner) )      //this camera belongs to some other mediaserver
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
                QnVirtualCameraResource* camRes = dynamic_cast<QnVirtualCameraResource*>(netRes);
                if (QnLiveStreamProvider::hasRunningLiveProvider(netRes)  || (camRes && !camRes->isScheduleDisabled())) {
                    if (res->getStatus() == Qn::Offline && !m_disconnectSended[uniqId]) {
                        QnVirtualCameraResourcePtr cam = res.dynamicCast<QnVirtualCameraResource>();
                        if (cam)
                            cam->issueOccured();
                        emit cameraDisconnected(res, qnSyncTime->currentUSecsSinceEpoch());
                        m_disconnectSended[uniqId] = true;
                    }
                } else {
                    res->setStatus(Qn::Offline);
                    m_resourceDiscoveryCounter[uniqId] = 0;
                }
            }
        }
        else {
            m_resourceDiscoveryCounter[uniqId] = 0;
            m_disconnectSended[uniqId] = false;
        }
    }
}

void QnMServerResourceDiscoveryManager::updateResourceStatus(const QnResourcePtr& res, QSet<QString>& discoveredResources)
{
    // seems like resource is in the pool and has OK ip
    QnNetworkResource* rpNetRes = dynamic_cast<QnNetworkResource*>(res.data());

    if (rpNetRes)
    {
        disconnect(rpNetRes, &QnResource::initAsyncFinished, this, &QnMServerResourceDiscoveryManager::onInitAsyncFinished);
        connect(rpNetRes, &QnResource::initAsyncFinished, this, &QnMServerResourceDiscoveryManager::onInitAsyncFinished);

        if (!rpNetRes->hasFlags(Qn::foreigner))
        {
            if (rpNetRes->getStatus() == Qn::Offline) 
            {
                // if resource with OK ip seems to be found; I do it coz if there is no readers and camera was offline and now online => status needs to be changed
                if (rpNetRes->getLastStatusUpdateTime().msecsTo(qnSyncTime->currentDateTime()) > 30)
                    rpNetRes->initAsync(false);
            }
            else if (!rpNetRes->isInitialized())
                rpNetRes->initAsync(false); // Resource already in resource pool. Try to init resource if resource is not authorized or not initialized by other reason
        }
        discoveredResources.insert(rpNetRes->getUniqueId());
        rpNetRes->setLastDiscoveredTime(qnSyncTime->currentDateTime());
    }

}

void QnMServerResourceDiscoveryManager::pingResources(const QnResourcePtr& res)
{
    if (m_runNumber%50 != 0)
        return;

    QnNetworkResource* rpNetRes = dynamic_cast<QnNetworkResource*>(res.data());
    if (rpNetRes)
    {
        if (!QnLiveStreamProvider::hasRunningLiveProvider(rpNetRes))
        {
            CLPing ping;
            ping.ping(rpNetRes->getHostAddress(), 1, 300);
        }
    }
}
