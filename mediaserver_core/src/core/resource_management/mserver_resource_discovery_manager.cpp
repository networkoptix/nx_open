#include "mserver_resource_discovery_manager.h"

#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/QThreadPool>

#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <nx/network/ping.h>
#include <nx/network/ip_range_checker.h>

#include "api/app_server_connection.h"
#include "business/business_event_connector.h"
#include "core/dataprovider/live_stream_provider.h"
#include <core/resource/camera_resource.h>
#include "core/resource/network_resource.h"
#include "core/resource/abstract_storage_resource.h"
#include "core/resource/storage_resource.h"
#include "core/resource_management/resource_pool.h"
#include <core/resource_management/resource_properties.h>
#include "core/resource_management/resource_searcher.h"
#include "plugins/storage/dts/abstract_dts_searcher.h"
#include "plugins/resource/desktop_camera/desktop_camera_resource.h"
#include "common/common_module.h"
#include "data_only_camera_resource.h"
#include "media_server/settings.h"

#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_camera_manager.h>

static const int NETSTATE_UPDATE_TIME = 1000 * 30;
static const int RETRY_COUNT_FOR_FOREIGN_RESOURCES = 2;

QnMServerResourceDiscoveryManager::QnMServerResourceDiscoveryManager()
{
    netStateTime.restart();
    connect(this, &QnMServerResourceDiscoveryManager::cameraDisconnected, qnBusinessRuleConnector, &QnBusinessEventConnector::at_cameraDisconnected);
    m_serverOfflineTimeout = MSSettings::roSettings()->value("redundancyTimeout", m_serverOfflineTimeout/1000).toInt() * 1000;
    m_serverOfflineTimeout = qMax(1000, m_serverOfflineTimeout);
    m_foreignResourcesRetryCount = 0;
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
    for(const QnResourcePtr& res: resources)
    {
        const QnNetworkResource* netRes = dynamic_cast<const QnNetworkResource*>(res.data());
        if (!netRes)
            continue;

        NX_LOG( lit("Discovery----: %1 %2").arg(netRes->getHostAddress()).arg(netRes->getName()), cl_logINFO);
    }

}

bool QnMServerResourceDiscoveryManager::processDiscoveredResources(QnResourceList& resources, SearchType searchType)
{
    // fill camera's ID

    {
        QnMutexLocker lock(&m_discoveryMutex);
        // check foreign resources several times in case if camera is discovered not quite stable. It'll improve redundant priority for foreign cameras
        for (auto itr = resources.begin(); itr != resources.end();)
        {
            QnSecurityCamResourcePtr camRes = (*itr).dynamicCast<QnSecurityCamResource>();
            if (!camRes) {
                ++itr;
                continue;
            }
            QnSecurityCamResourcePtr existRes = qnResPool->getResourceByUniqueId<QnSecurityCamResource>((*itr)->getUniqueId());
            if (existRes && existRes->hasFlags(Qn::foreigner) && !existRes->hasFlags(Qn::desktop_camera))
            {
                m_tmpForeignResources.insert(camRes->getUniqueId(), camRes);
                itr = resources.erase(itr);
            }
            else {
                ++itr;
            }
        }
        if (++m_foreignResourcesRetryCount >= RETRY_COUNT_FOR_FOREIGN_RESOURCES)
        {
            m_foreignResourcesRetryCount = 0;

            // sort foreign resources to add more important cameras first: check if it is an own cameras, then check failOver priority order
            auto foreignResources = m_tmpForeignResources.values();
            const QnUuid ownGuid = qnCommon->moduleGUID();
            std::sort(foreignResources.begin(), foreignResources.end(), [&ownGuid](const QnSecurityCamResourcePtr& leftCam, const QnSecurityCamResourcePtr& rightCam)
            {
                bool leftOwnServer = leftCam->preferredServerId() == ownGuid;
                bool rightOwnServer = rightCam->preferredServerId() == ownGuid;
                if (leftOwnServer != rightOwnServer)
                    return leftOwnServer > rightOwnServer;

                // arrange cameras by failover priority order
                return leftCam->failoverPriority() > rightCam->failoverPriority();
            });

            for (const auto& res : foreignResources)
                resources << res;
            m_tmpForeignResources.clear();
        }
    }

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

        DLOG(lit("%1 processing %2 resources")
                .arg(FL1(Q_FUNC_INFO))
                .arg(resources.size()));

        QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();
        if (!newNetRes) {
            //TODO: #rvasilenko please make sure we can safely continue here
            //it = resources.erase(it); // do not need to investigate OK resources
            ++it;
            continue;
        }

        DLOG(lit("%1 Processing resource %2")
                .arg(FL1(Q_FUNC_INFO))
                .arg(NetResString(newNetRes)));

        QnResourcePtr rpResource = QnResourceDiscoveryManager::findSameResource(newNetRes);
        QnVirtualCameraResourcePtr newCamRes = newNetRes.dynamicCast<QnVirtualCameraResource>();

        if (!rpResource)
        {
            if (newCamRes && newCamRes->needCheckIpConflicts())
            {
                // do not count 2--N channels of multichannel cameras as conflict
                quint32 ips = resolveAddress(newNetRes->getHostAddress()).toIPv4Address();
                if (ips)
                    ipsList[ips].insert(newNetRes);
            }

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

        const bool isForeign = rpResource->hasFlags(Qn::foreigner);
        QnVirtualCameraResourcePtr existCamRes = rpNetRes.dynamicCast<QnVirtualCameraResource>();
        if (existCamRes)
        {
            QnUuid newTypeId = newNetRes->getTypeId();
            bool updateTypeId = existCamRes->getTypeId() != newNetRes->getTypeId();

            DLOG(lit("%1 Found existing cam res %1 for new resource %2")
                    .arg(FL1(Q_FUNC_INFO))
                    .arg(NetResString(rpNetRes))
                    .arg(NetResString(newNetRes)));

            newNetRes->setPhysicalId(rpNetRes->getUniqueId());
            if (rpNetRes->mergeResourcesIfNeeded(newNetRes) || isForeign || updateTypeId)
            {
                if (isForeign || updateTypeId)
                {
                    //preserving "manuallyAdded" flag
                    const bool isDiscoveredManually = newCamRes->isManuallyAdded();
                    newNetRes->update(existCamRes);
                    newCamRes->setManuallyAdded( isDiscoveredManually );

                    newNetRes->setParentId(qnCommon->moduleGUID());
                    newNetRes->setFlags(existCamRes->flags() & ~Qn::foreigner);
                    newNetRes->setId(existCamRes->getId());
                    newNetRes->addFlags(Qn::parent_change);
                    if (updateTypeId)
                        newNetRes->setTypeId(newTypeId);
                    extraResources << newNetRes;
                }
                else
                {
                    ec2::ApiCameraData apiCamera;
                    fromResourceToApi(existCamRes, apiCamera);

                    ec2::AbstractECConnectionPtr connect = QnAppServerConnectionFactory::getConnection2();
                    const ec2::ErrorCode errorCode = connect->getCameraManager(Qn::kSystemAccess)->addCameraSync(apiCamera);
                    if( errorCode != ec2::ErrorCode::ok )
                        NX_LOG( QString::fromLatin1("Discovery----: Can't add camera to ec2. %1").arg(ec2::toString(errorCode)), cl_logWARNING );
                    propertyDictionary->saveParams( existCamRes->getId() );
                }
            }
        }

        // seems like resource is in the pool and has OK ip
        updateResourceStatus(rpNetRes);

        discoveredResources.insert(rpNetRes->getUniqueId());
        rpNetRes->setLastDiscoveredTime(qnSyncTime->currentDateTime());
#ifndef EDGE_SERVER
        pingResources(rpNetRes);
#endif
        it = resources.erase(it); // do not need to investigate OK resources
    }

    // ========================= send conflict info =====================
    for (QMap<quint32, QSet<QnNetworkResourcePtr> >::iterator itr = ipsList.begin(); itr != ipsList.end(); ++itr)
    {
        if (itr.value().size() > 1)
        {
            QHostAddress hostAddr(itr.key());
            QStringList conflicts;
            for(const QnNetworkResourcePtr& camRes: itr.value())
            {
                conflicts << camRes->getMAC().toString();
                QnVirtualCameraResource* cam = dynamic_cast<QnVirtualCameraResource*>(camRes.data());
                if (cam)
                    cam->issueOccured();
            }
            emit CameraIPConflict(hostAddr, conflicts);
        }
    }

    // ==========if resource is not discovered last minute and we do not record it and do not see live => readers not runing
    if (searchType == SearchType::Full)
        markOfflineIfNeeded(discoveredResources);
    // ======================

    bool foundSmth = !resources.isEmpty();
    if (foundSmth)
        NX_LOG( lit("Discovery----: after excluding existing resources we've got %1 new resources:").arg(resources.size()), cl_logINFO);

    printInLogNetResources(resources);

    if (resources.isEmpty())
    {
        resources << extraResources;
        return true;
    }

    resources << extraResources;

    if (resources.size())
    {
        NX_LOG("Discovery----: Final result: ", cl_logINFO);
        printInLogNetResources(resources);
    }
    return true;
}

void QnMServerResourceDiscoveryManager::markOfflineIfNeeded(QSet<QString>& discoveredResources)
{
    const QnResourceList& resources = qnResPool->getResources();

    for(const QnResourcePtr& res: resources)
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

void QnMServerResourceDiscoveryManager::updateResourceStatus(const QnNetworkResourcePtr& rpNetRes)
{
    if (!rpNetRes->isInitialized() && !rpNetRes->hasFlags(Qn::foreigner))
        rpNetRes->initAsync(false); // wait for initialization
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
