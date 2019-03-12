#include "mserver_resource_discovery_manager.h"

#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/QThreadPool>

#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/utils/log/log.h>
#include <nx/network/ping.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <nx/vms/server/event/event_connector.h>
#include <providers/live_stream_provider.h>
#include <core/resource/camera_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/abstract_storage_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/resource_searcher.h>
#include <core/resource_management/data_only_camera_resource.h>
#include <plugins/resource/desktop_camera/desktop_camera_resource.h>
#include <common/common_module.h>
#include <media_server/settings.h>

#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/managers/abstract_camera_manager.h>
#include <media_server/media_server_module.h>
#include <core/resource/general_attribute_pool.h>
#include <core/resource/camera_user_attribute_pool.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>

namespace {

static const int RETRY_COUNT_FOR_FOREIGN_RESOURCES = 2;
static const int kRetryCountToMakeCamOffline = 3;
static const int kMinServerStartupTimeToTakeForeignCamerasMs = 1000 * 60;

template<class ResourcePointer>
QString netResourceString(const ResourcePointer& res)
{
    return QString("Network resource url: %1, ip: %2, mac: %3, uniqueId: %4")
        .arg(res->getUrl())
        .arg(res->getHostAddress())
        .arg(res->getMAC().toString())
        .arg(res->getUniqueId());
}

} // namespace

QnMServerResourceDiscoveryManager::QnMServerResourceDiscoveryManager(
    QnMediaServerModule* serverModule)
    :
    QnResourceDiscoveryManager(serverModule->commonModule()),
    m_serverModule(serverModule)
{
    connect(resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        &QnMServerResourceDiscoveryManager::at_resourceDeleted,
        Qt::DirectConnection);
    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this,
        &QnMServerResourceDiscoveryManager::at_resourceAdded,
        Qt::DirectConnection);
    connect(commonModule()->globalSettings(),
        &QnGlobalSettings::disabledVendorsChanged,
        this,
        &QnMServerResourceDiscoveryManager::updateSearchersUsage);
    connect(commonModule()->globalSettings(),
        &QnGlobalSettings::autoDiscoveryChanged,
        this,
        &QnMServerResourceDiscoveryManager::updateSearchersUsage);

    connect(
        this, &QnMServerResourceDiscoveryManager::cameraDisconnected,
        serverModule->eventConnector(),
        &nx::vms::server::event::EventConnector::at_cameraDisconnected);
    m_serverOfflineTimeout = serverModule->settings().redundancyTimeout() * 1000;
    m_serverOfflineTimeout = qMax(1000, m_serverOfflineTimeout);
    m_startupTimer.restart();
}

QnMServerResourceDiscoveryManager::~QnMServerResourceDiscoveryManager()
{
    stop();
}

void QnMServerResourceDiscoveryManager::stop()
{
    base_type::stop();
    resourcePool()->disconnect(this);
    commonModule()->globalSettings()->disconnect(this);
    this->disconnect(this);
}

QnResourcePtr QnMServerResourceDiscoveryManager::createResource(const QnUuid& resourceTypeId,
    const QnResourceParams& params)
{
    QnResourcePtr res = QnResourceDiscoveryManager::createResource(resourceTypeId, params);
    if (res)
        return res;

    const QnResourceTypePtr& resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (!resourceType)
        return res;

    // Found resource type, but could not find factory. Disabled discovery?
    return QnResourcePtr(new DataOnlyCameraResource(resourceTypeId));
}

static void printInLogNetResources(const QnResourceList& resources)
{
    for (const QnResourcePtr& res: resources)
    {
        const QnNetworkResource* netRes = dynamic_cast<const QnNetworkResource*>(res.data());
        if (!netRes)
            continue;

        NX_INFO(typeid(QnMServerResourceDiscoveryManager),
            "%1 %2", netRes->getHostAddress(), netRes->getName());
    }
}

void QnMServerResourceDiscoveryManager::sortForeignResources(
    QList<QnSecurityCamResourcePtr>& foreignResources)
{
    const QnUuid ownGuid = commonModule()->moduleGUID();
    std::sort(foreignResources.begin(), foreignResources.end(),
        [&ownGuid](const QnSecurityCamResourcePtr& left, const QnSecurityCamResourcePtr& right)
    {
        auto failoverPriority =
            [](const QnSecurityCamResourcePtr& camera)
            {
                QnCameraUserAttributePool::ScopedLock userAttributesLock(
                    camera->commonModule()->cameraUserAttributesPool(),
                    nx::vms::api::CameraData::physicalIdToId(camera->getPhysicalId()));
                return (*userAttributesLock)->failoverPriority;
            };
        auto preferredServerId =
            [](const QnSecurityCamResourcePtr& camera)
            {
                QnCameraUserAttributePool::ScopedLock userAttributesLock(
                    camera->commonModule()->cameraUserAttributesPool(),
                    nx::vms::api::CameraData::physicalIdToId(camera->getPhysicalId()));
                return (*userAttributesLock)->preferredServerId;
            };

        bool isLeftOwnServer = preferredServerId(left) == ownGuid;
        bool isRightOwnServer = preferredServerId(right) == ownGuid;
        if (isLeftOwnServer != isRightOwnServer)
            return isLeftOwnServer > isRightOwnServer;

        return failoverPriority(left) > failoverPriority(right);
    });
}

bool QnMServerResourceDiscoveryManager::processDiscoveredResources(QnResourceList& resources,
    SearchType searchType)
{
    // fill camera's ID
    {
        QnMutexLocker lock(&m_discoveryMutex);
        int foreignResourceIndex = m_discoveryCounter % RETRY_COUNT_FOR_FOREIGN_RESOURCES;
        while (m_tmpForeignResources.size() <= foreignResourceIndex)
            m_tmpForeignResources.push_back(ResourceList());
        m_tmpForeignResources[foreignResourceIndex].clear();

        // Check foreign resources several times in case if camera is discovered not quite stable.
        // It will improve redundant priority for foreign cameras.
        for (auto itr = resources.begin(); itr != resources.end();)
        {
            QnSecurityCamResourcePtr camRes = (*itr).dynamicCast<QnSecurityCamResource>();
            if (!camRes)
            {
                ++itr;
                continue;
            }
            QnSecurityCamResourcePtr existRes =
                resourcePool()->getResourceByUniqueId<QnSecurityCamResource>((*itr)->getUniqueId());

            if (existRes && existRes->hasFlags(Qn::foreigner)
                && !existRes->hasFlags(Qn::desktop_camera))
            {
                NX_VERBOSE(this,
                    "Filter resource %1 because it is foreign. It will be checked later.",
                    netResourceString(existRes));
                m_tmpForeignResources[foreignResourceIndex].insert(camRes->getUniqueId(), camRes);
                itr = resources.erase(itr);
            }
            else
            {
                ++itr;
            }
        }
        if (m_tmpForeignResources.size() == RETRY_COUNT_FOR_FOREIGN_RESOURCES
            && m_startupTimer.elapsed() > kMinServerStartupTimeToTakeForeignCamerasMs)
        {
            // Exclude duplicates.
            ResourceList foreignResourcesMap;
            for (const auto& resList: m_tmpForeignResources)
            {
                for (auto itr = resList.begin(); itr != resList.end(); ++itr)
                    foreignResourcesMap.insert(itr.key(), itr.value());
            }

            // Sort foreign resources to add more important cameras first: check if it is
            // own cameras, then check failOver priority order.
            auto foreignResources = foreignResourcesMap.values();
            sortForeignResources(foreignResources);
            for (const auto& res: foreignResources)
            {
                NX_VERBOSE(this, "Add foreign resource %1 to check", netResourceString(res));
                resources << res;
            }
        }
    }

    QnResourceList extraResources;
    QSet<QString> discoveredResources;

    // Assemble list of existing ip.
    QMap<quint32, QSet<QnNetworkResourcePtr> > ipsList;

    // Excluding already existing resources.
    QnResourceList::iterator it = resources.begin();
    while (it != resources.end())
    {
        if (needToStop())
            return false;

        NX_VERBOSE(this, "%1 processing %2 resources", __func__, resources.size());

        QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();
        if (!newNetRes)
        {
            // TODO: #rvasilenko please make sure we can safely continue here
            //it = resources.erase(it); // do not need to investigate OK resources
            ++it;
            continue;
        }

        NX_VERBOSE(this, "%1 Processing resource [%2]", __func__, netResourceString(newNetRes));

        QnResourcePtr rpResource = QnResourceDiscoveryManager::findSameResource(newNetRes);
        QnVirtualCameraResourcePtr newCamRes = newNetRes.dynamicCast<QnVirtualCameraResource>();

        // Handle newly discovered resources.
        if (!rpResource)
        {
            if (shouldAddNewlyDiscoveredResource(newNetRes))
                ++it;
            else
                it = resources.erase(it);

            continue;
        }

        if (rpResource->hasFlags(Qn::foreigner))
        {
            if (!canTakeForeignCamera(rpResource.dynamicCast<QnSecurityCamResource>(), extraResources.size()))
            {
                NX_VERBOSE(this, "Can't take foreign resource [%1] now", netResourceString(newNetRes));
                it = resources.erase(it); //< do not touch foreign resource
                continue;
            }
        }

        if (newCamRes)
        {
            quint32 ips = nx::network::resolveAddress(newNetRes->getHostAddress()).toIPv4Address();
            if (ips)
                ipsList[ips].insert(newNetRes);
        }

        QnNetworkResourcePtr rpNetRes = rpResource.dynamicCast<QnNetworkResource>();

        const bool isForeign = rpResource->hasFlags(Qn::foreigner);
        QnVirtualCameraResourcePtr existCamRes = rpNetRes.dynamicCast<QnVirtualCameraResource>();
        if (existCamRes)
        {
            QnUuid newTypeId = newNetRes->getTypeId();
            bool updateTypeId = existCamRes->getTypeId() != newNetRes->getTypeId();

            NX_VERBOSE(this, "%1 Found existing cam res [%2] for new resource [%3]",
                __func__, netResourceString(rpNetRes), netResourceString(newNetRes));

            newNetRes->setPhysicalId(rpNetRes->getUniqueId());
            if (rpNetRes->mergeResourcesIfNeeded(newNetRes) || isForeign || updateTypeId)
            {
                if (isForeign || updateTypeId)
                {
                    //preserving "manuallyAdded" flag
                    const bool isDiscoveredManually = newCamRes->isManuallyAdded();
                    newNetRes->update(existCamRes);
                    newCamRes->setManuallyAdded(isDiscoveredManually);
                    NX_ASSERT(newNetRes->resourcePool() == nullptr);
                    newNetRes->setParentId(commonModule()->moduleGUID());
                    newNetRes->setFlags(existCamRes->flags() & ~Qn::foreigner);
                    newNetRes->setId(existCamRes->getId());
                    newNetRes->addFlags(Qn::parent_change);
                    if (updateTypeId)
                        newNetRes->setTypeId(newTypeId);

                    NX_VERBOSE(this, "Add foreign resource [%1] to search list",
                        netResourceString(rpNetRes));

                    extraResources << newNetRes;
                }
                else
                {
                    NX_VERBOSE(this, "Merge existing resource with searched resource [%1]",
                        netResourceString(rpNetRes));

                    nx::vms::api::CameraData apiCamera;
                    ec2::fromResourceToApi(existCamRes, apiCamera);
                    apiCamera.id = nx::vms::api::CameraData::physicalIdToId(apiCamera.physicalId);

                    ec2::AbstractECConnectionPtr connect = commonModule()->ec2Connection();
                    const ec2::ErrorCode errorCode =
                        connect->getCameraManager(Qn::kSystemAccess)->addCameraSync(apiCamera);
                    if (errorCode != ec2::ErrorCode::ok)
                        NX_WARNING(this, "Can't add camera to ec2. %1",ec2::toString(errorCode));
                    existCamRes->saveProperties();
                }
            }
        }

        // Seems like resource is in the pool and has OK ip.
        updateResourceStatus(rpNetRes);

        discoveredResources.insert(rpNetRes->getUniqueId());
        rpNetRes->setLastDiscoveredTime(qnSyncTime->currentDateTime());
#ifndef EDGE_SERVER
        pingResources(rpNetRes);
#endif
        it = resources.erase(it); // do not need to investigate OK resources
    }

    // ========================= send conflict info =====================
    for (QMap<quint32, QSet<QnNetworkResourcePtr> >::iterator itr = ipsList.begin();
        itr != ipsList.end(); ++itr)
    {
        if (hasIpConflict(itr.value()))
        {
            QHostAddress hostAddr(itr.key());
            QStringList conflicts;
            for(const QnNetworkResourcePtr& camRes: itr.value())
            {
                // Human-readable value to help user identify a camera.
                QString cameraId = camRes->getMAC().toString();
                if (cameraId.isEmpty())
                    cameraId = camRes->getId().toString();

                conflicts << cameraId;
                if (auto camera = camRes.dynamicCast<QnVirtualCameraResource>())
                    camera->issueOccured();
            }
            emit CameraIPConflict(hostAddr, conflicts);
        }
    }

    // If resource is not discovered last minute and we do not record it and do not see live
    // then readers are not running.
    if (searchType == SearchType::Full)
        markOfflineIfNeeded(discoveredResources);

    bool foundSmth = !resources.isEmpty();
    if (foundSmth)
    {
        NX_INFO(this,
            "After excluding existing resources we've got %1 new resources:", resources.size());
    }

    printInLogNetResources(resources);
    ++m_discoveryCounter;
    if (resources.isEmpty())
    {
        resources << extraResources;
        return true;
    }

    resources << extraResources;

    if (resources.size())
    {
        NX_INFO(this, "Final result: ");
        printInLogNetResources(resources);
    }
    return true;
}

nx::vms::common::AnalyticsPluginResourcePtr
    QnMServerResourceDiscoveryManager::createAnalyticsPluginResource(
        const QnResourceParams& /*parameters*/)
{
    return nx::vms::server::resource::AnalyticsPluginResourcePtr(
        new nx::vms::server::resource::AnalyticsPluginResource(m_serverModule));
}

nx::vms::common::AnalyticsEngineResourcePtr
    QnMServerResourceDiscoveryManager::createAnalyticsEngineResource(
        const QnResourceParams& /*parameters*/)
{
    return nx::vms::server::resource::AnalyticsEngineResourcePtr(
        new nx::vms::server::resource::AnalyticsEngineResource(m_serverModule));
}

void QnMServerResourceDiscoveryManager::at_resourceAdded(const QnResourcePtr & resource)
{
    const QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (server)
    {
        connect(server,
            &QnMediaServerResource::redundancyChanged,
            this,
            &QnMServerResourceDiscoveryManager::updateSearchersUsage);
        updateSearchersUsage();
    }

    std::vector<QnManualCameraInfo> newCameras;
    {
        QnMutexLocker lock(&m_searchersListMutex);
        const auto camera = resource.dynamicCast<QnSecurityCamResource>();
        if (!camera || !camera->isManuallyAdded())
            return;

        if (!m_manualCameraByUniqueId.contains(camera->getUniqueId()))
            newCameras.push_back(manualCameraInfoUnsafe(camera));
    }

    if (!newCameras.empty())
        registerManualCameras(newCameras);
}

void QnMServerResourceDiscoveryManager::at_resourceDeleted(const QnResourcePtr & resource)
{
    const QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (server)
    {
        disconnect(server, nullptr, this, nullptr);
        updateSearchersUsage();
    }

    QnMutexLocker lock(&m_searchersListMutex);
    m_manualCameraByUniqueId.remove(resource->getUniqueId());
    m_recentlyDeleted << resource->getUniqueId();

    NX_DEBUG(this, "Manual camera %1 is deleted on %2",
        resource->getUniqueId(), resource->getUrl());
}

bool QnMServerResourceDiscoveryManager::hasIpConflict(const QSet<QnNetworkResourcePtr>& cameras)
{
    if (cameras.size() < 2)
        return false;
    QSet<QString> cameraGroups;
    QSet<int> portList;

    for (const auto& netRes: cameras)
    {
        auto camera = netRes.dynamicCast<QnSecurityCamResource>();
        if (!camera || !camera->needCheckIpConflicts())
            continue;

        QString groupId = camera->getGroupId().isEmpty()
            ? camera->getPhysicalId()
            : camera->getGroupId();
        cameraGroups << groupId;
        portList << QUrl(camera->getUrl()).port();

        if (cameraGroups.size() < 2)
            continue; //< Several cameras with same group so far

        if (!camera->isManuallyAdded())
            return true; //< Conflict detected
        if (portList.size() < cameraGroups.size())
        {
            // For manually added cameras it is allowed to have several cameras
            // with same IP but different ports.
            return true;
        }
    }

    return false;
}

void QnMServerResourceDiscoveryManager::markOfflineIfNeeded(QSet<QString>& discoveredResources)
{
    const QnResourceList& resources = resourcePool()->getResources();

    for(const QnResourcePtr& res: resources)
    {
        QnNetworkResource* netRes = dynamic_cast<QnNetworkResource*>(res.data());
        if (!netRes)
            continue;

        if (res->hasFlags(Qn::server_live_cam)) //< this is camera from mediaserver on the client
            continue;

        if (res->hasFlags(Qn::foreigner)) //< this camera belongs to some other mediaserver
            continue;

        QDateTime ldt = netRes->getLastDiscoveredTime();
        QDateTime currentT = qnSyncTime->currentDateTime();

        //check if resource was discovered
        QString uniqId = res->getUniqueId();
        if (!discoveredResources.contains(uniqId))
        {
            // resource is not found
            m_resourceDiscoveryCounter[uniqId]++;

            if (m_resourceDiscoveryCounter[uniqId] >= kRetryCountToMakeCamOffline)
            {
                QnVirtualCameraResource* camRes = dynamic_cast<QnVirtualCameraResource*>(netRes);
                if (QnLiveStreamProvider::hasRunningLiveProvider(netRes)
                    || (camRes && camRes->isLicenseUsed()))
                {
                    if (res->getStatus() == Qn::Offline && !m_disconnectSended[uniqId]) {
                        QnVirtualCameraResourcePtr cam = res.dynamicCast<QnVirtualCameraResource>();
                        if (cam)
                            cam->issueOccured();
                        emit cameraDisconnected(res, qnSyncTime->currentUSecsSinceEpoch());
                        m_disconnectSended[uniqId] = true;
                    }
                }
                else
                {
                    NX_INFO(this,
                        "Mark resource %1 as offline"
                        " because it doesn't response to discovery any more.",
                        netResourceString(camRes));

                    res->setStatus(Qn::Offline);
                    m_resourceDiscoveryCounter[uniqId] = 0;
                }
            }
        }
        else
        {
            m_resourceDiscoveryCounter[uniqId] = 0;
            m_disconnectSended[uniqId] = false;
        }
    }
}

void QnMServerResourceDiscoveryManager::updateResourceStatus(const QnNetworkResourcePtr& rpNetRes)
{
    if (!rpNetRes->isInitialized() && !rpNetRes->hasFlags(Qn::foreigner))
    {
        NX_VERBOSE(this, "Trying to init not initialized resource [%1]", rpNetRes);
        rpNetRes->initAsync(false); //< wait for initialization
    }
}

bool QnMServerResourceDiscoveryManager::shouldAddNewlyDiscoveredResource(
    const QnNetworkResourcePtr &newResource) const
{
    const auto newCameraResource = newResource.dynamicCast<QnVirtualCameraResource>();

    if (!newCameraResource)
        return true;

    if (newCameraResource->isManuallyAdded())
        return true;

    const auto knownCameraChannels =
        resourcePool()->getResourcesBySharedId(newCameraResource->getSharedId());
    if (knownCameraChannels.empty())
        return true;

    // If there is another channel for the camera on this server, we can add another one.
    const auto it = std::find_if(knownCameraChannels.begin(), knownCameraChannels.end(),
        [this](const QnSecurityCamResourcePtr& camera)
        {
            return camera->getParentId() == commonModule()->moduleGUID();
        });
    if (it != knownCameraChannels.end())
        return true;

    NX_VERBOSE(this, "Other channels of resource '%1' belong to other servers, do nothing.",
        netResourceString(newCameraResource));
    return false;
}

void QnMServerResourceDiscoveryManager::pingResources(const QnResourcePtr& res)
{
    if (m_runNumber % 50 != 0)
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
