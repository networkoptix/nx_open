// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_discovery_manager.h"

#include <exception>
#include <list>
#include <set>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThreadPool>

#include <api/runtime_info_manager.h>
#include <core/resource/abstract_storage_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/storage_plugin_factory.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_management_ini.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_searcher.h>
#include <nx/network/nettools.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/util.h>

using namespace nx::vms::common;

// ------------------------------------ QnManualCameraInfo -----------------------------

QnManualCameraInfo::QnManualCameraInfo(const nx::utils::Url& url, const QAuthenticator& auth, const QString& resType, const QString& physicalId)
{
    QString urlStr = url.toString();
    this->url = url;
    this->auth = auth;
    this->resType = qnResTypePool->getResourceTypeByName(resType);
    this->searcher = 0;
    this->physicalId = physicalId;
}

QnResourceDiscoveryManagerTimeoutDelegate::QnResourceDiscoveryManagerTimeoutDelegate( QnResourceDiscoveryManager* discoveryManager)
:
    m_discoveryManager(discoveryManager)
{
}

void QnResourceDiscoveryManagerTimeoutDelegate::onTimeout()
{
    m_discoveryManager->doResourceDiscoverIteration();
}

// ------------------------------------ QnResourceDiscoveryManager -----------------------------

QnResourceDiscoveryManager::QnResourceDiscoveryManager(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    nx::vms::common::SystemContextAware(systemContext),
    m_ready(false),
    m_state(InitialSearch),
    m_discoveryUpdateIdx(0),
    m_manualCameraListChanged(false)
{
    m_threadPool.setMaxThreadCount(resourceManagementIni().maxResourceDiscoveryThreadCount);
}

QnResourceDiscoveryManager::~QnResourceDiscoveryManager()
{
    stop();
    m_threadPool.waitForDone();
}

void QnResourceDiscoveryManager::setReady(bool ready)
{
    m_ready = ready;
}

void QnResourceDiscoveryManager::start(Priority priority)
{
    updateSearchersUsage();
    QnLongRunnable::start(priority);
    //moveToThread(this);
}

void QnResourceDiscoveryManager::addDeviceSearcher(
    QnAbstractResourceSearcher* searcher, bool pushFront)
{
    NX_MUTEX_LOCKER lock(&m_searchersListMutex);
    if (pushFront)
        m_searchersList.push_front(searcher);
    else
        m_searchersList.push_back(searcher);
}

void QnResourceDiscoveryManager::setResourceProcessor(QnResourceProcessor* processor)
{
    NX_MUTEX_LOCKER lock(&m_searchersListMutex);
    m_resourceProcessor = processor;
}

QnAbstractResourceSearcher* QnResourceDiscoveryManager::searcherByManufacturer(
    const QString& manufacturer) const
{
    NX_MUTEX_LOCKER lock(&m_searchersListMutex);
    for (const auto& searcher: m_searchersList)
    {
        if (searcher && searcher->manufacturer() == manufacturer)
            return searcher;
    }

    return nullptr;
}

QnResourcePtr QnResourceDiscoveryManager::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
{
    QnResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return result;

    if (resourceTypeId == nx::vms::api::StorageData::kResourceTypeId)
    {
        result = QnResourcePtr(appContext()->storagePluginFactory()->createStorage(
            params.url, resourceManagementIni().allowDefaultStorageFactory));
    }
    else if (resourceTypeId == nx::vms::api::AnalyticsPluginData::kResourceTypeId)
    {
        result = createAnalyticsPluginResource(params);
    }
    else if (resourceTypeId == nx::vms::api::AnalyticsEngineData::kResourceTypeId)
    {
        result = createAnalyticsEngineResource(params);
    }
    else
    {
        ResourceSearcherList searchersList;
        {
            NX_MUTEX_LOCKER lock(&m_searchersListMutex);
            searchersList = m_searchersList;
        }

        for (QnAbstractResourceSearcher *searcher: searchersList)
        {
            result = searcher->createResource(resourceTypeId, params);
            if (!result.isNull())
                break;
        }
    }

    return result;
}

void QnResourceDiscoveryManager::stop()
{
    pleaseStop();
    wait();
    NX_MUTEX_LOCKER lock(&m_searchersListMutex);
    m_searchersList.clear();

    for (auto& resourceList: m_lastDiscoveredResources)
        resourceList.clear();
}

void QnResourceDiscoveryManager::pleaseStop()
{
    NX_DEBUG(this, "Please Stop");
    if (isRunning())
    {
        NX_MUTEX_LOCKER lock(&m_searchersListMutex);
        for (QnAbstractResourceSearcher* searcher: m_searchersList)
            searcher->pleaseStop();
    }
    base_type::pleaseStop();
    quit(); //telling thread's event loop to return
}

void QnResourceDiscoveryManager::run()
{
    initSystemThreadId();
    m_runNumber = 0;
    // #dkargin: I really want to move m_timer inside QnResourceDiscoveryManagerTimeoutDelegate.
    m_timer.reset(new QTimer());
    m_timer->setSingleShot(true);

    m_state = InitialSearch;

    QnResourceDiscoveryManagerTimeoutDelegate timoutDelegate(this);

    connect(m_timer.get(), &QTimer::timeout,
        &timoutDelegate, &QnResourceDiscoveryManagerTimeoutDelegate::onTimeout);

    m_timer->start(0);    //immediate execution

    exec();

    m_timer.reset();
}

static const int GLOBAL_DELAY_BETWEEN_CAMERA_SEARCH_MS = 1000;

void QnResourceDiscoveryManager::doInitialSearch()
{
    ResourceSearcherList searchersList;
    {
        NX_MUTEX_LOCKER lock(&m_searchersListMutex);
        searchersList = m_searchersList;
    }

    for (QnAbstractResourceSearcher* searcher: searchersList)
    {
        if ((searcher->discoveryMode() != DiscoveryMode::disabled) && searcher->isLocal())
        {
            QnResourceList lst = searcher->search();
            lst = remapPhysicalIdIfNeed(lst); //< Used in case of device replacement.
            m_resourceProcessor->processResources(lst);
        }
    }
    emit localSearchDone();
}

void QnResourceDiscoveryManager::doResourceDiscoverIteration()
{
    QElapsedTimer discoveryTime;
    discoveryTime.restart();
    int delayForNextSearch = 0;
    switch( m_state )
    {
        case InitialSearch:
            doInitialSearch();
            m_state = PeriodicSearch;
            break;

        case PeriodicSearch:
            if (!m_ready)
                break;

            updateLocalNetworkInterfaces();

            if (!m_resourceProcessor->isBusy())
            {
                QnResourceList result = findNewResources();
                if (!result.isEmpty())
                    m_resourceProcessor->processResources(result);
            }

            ++m_runNumber;
            delayForNextSearch = qMax(GLOBAL_DELAY_BETWEEN_CAMERA_SEARCH_MS,
                int(resourceManagementIni().cameraDiscoveryIntervalMs - discoveryTime.elapsed()));
            break;
    }

    m_timer->start(delayForNextSearch);
}

void QnResourceDiscoveryManager::setLastDiscoveredResources(const QnResourceList& resources)
{
    NX_MUTEX_LOCKER lock(&m_resListMutex);
    m_lastDiscoveredResources[m_discoveryUpdateIdx] = resources;
    m_discoveryUpdateIdx = (m_discoveryUpdateIdx + 1) % m_lastDiscoveredResources.size();
}

QnResourceList QnResourceDiscoveryManager::lastDiscoveredResources() const
{
    NX_MUTEX_LOCKER lock(&m_resListMutex);
    QMap<QString, QnResourcePtr> result;
    for (const QnResourceList& resList: m_lastDiscoveredResources)
    {
        for (const QnResourcePtr& res: resList)
        {
            QString key;
            if (auto networkResource = res.dynamicCast<QnNetworkResource>())
                key = networkResource->getPhysicalId();
            else
                key = res->getUrl();

            result.insert(key, res);
        }
    }
    return result.values();
}

void QnResourceDiscoveryManager::updateLocalNetworkInterfaces()
{
    const nx::network::AddressFilters addressMask =
        nx::network::AddressFilter::ipV4
        | nx::network::AddressFilter::ipV6
        | nx::network::AddressFilter::noLocal
        | nx::network::AddressFilter::noLoopback;

    auto localAddresses = nx::network::allLocalAddresses(addressMask);
    if (!m_allLocalAddresses)
    {
        NX_DEBUG(this, "Network addresses initial: %1", nx::containerString(localAddresses));
    }
    else if (*m_allLocalAddresses != localAddresses)
    {
        NX_DEBUG(this, "Network addresses changed: %1", nx::containerString(localAddresses));
        emit localInterfacesChanged();
    }
    else
    {
        NX_DEBUG(this, "Network addresses are up to date: %1", nx::containerString(localAddresses));
        return;
    }

    m_allLocalAddresses = localAddresses;
}

nx::vms::common::AnalyticsPluginResourcePtr
    QnResourceDiscoveryManager::createAnalyticsPluginResource(
        const QnResourceParams& /*parameters*/)
{
    using namespace nx::vms::common;
    return AnalyticsPluginResourcePtr(new AnalyticsPluginResource());
}

nx::vms::common::AnalyticsEngineResourcePtr
    QnResourceDiscoveryManager::createAnalyticsEngineResource(
        const QnResourceParams& /*parameters*/)
{
    using namespace nx::vms::common;
    return AnalyticsEngineResourcePtr(new AnalyticsEngineResource());
}

QnResourceList QnResourceDiscoveryManager::findNewResources()
{
    std::list<std::pair<QnResourcePtr, QnAbstractResourceSearcher*>> resourcesAndSearches;
    std::set<QString> resourcePhysicalIDs;    //used to detect duplicate resources (same resource found by multiple drivers)
    m_searchersListMutex.lock();
    ResourceSearcherList searchersList = m_searchersList;
    m_searchersListMutex.unlock();
    auto searchType = SearchType::Full;
    for (QnAbstractResourceSearcher *searcher: searchersList)
    {
        if ((searcher->discoveryMode() != DiscoveryMode::disabled) && !needToStop())
        {
            QElapsedTimer timer;
            timer.restart();
            QnResourceList lst = searcher->search();
            lst = remapPhysicalIdIfNeed(lst); //< Used in case of device replacement.
            NX_DEBUG(this, lit("Searcher %1 took %2 ms to find %3 resources").
                arg(searcher->manufacturer())
                .arg(timer.elapsed())
                .arg(lst.size()));

            // resources can be searched by client in or server.
            // if this client in stand alone => lets add Qn::local
            // server does not care about flags
            for(QnResourceList::iterator it = lst.begin(); it != lst.end();)
            {
                const QnSecurityCamResource* camRes = dynamic_cast<QnSecurityCamResource*>(it->data());
                if (camRes && !isCameraAllowed(searcher->manufacturer(), camRes))
                {
                    it = lst.erase( it );
                    continue;
                }

                const QnNetworkResource* networkRes = dynamic_cast<QnNetworkResource*>(it->data());
                if( networkRes )
                {
                    //checking that resource do not duplicate already found ones
                    if( !resourcePhysicalIDs.insert( networkRes->getPhysicalId() ).second )
                    {
                        it = lst.erase( it );
                        continue;   //resource with such unique id is already present
                    }
                }

                if( searcher->isLocal() )
                    (*it)->addFlags(Qn::local);

                ++it;
            }

            for( QnResourcePtr& res: lst )
                resourcesAndSearches.push_back( std::make_pair( std::move(res), searcher ) );
        }
        else
        {
            searchType = SearchType::Partial;
        }
    }
    const auto& resPool = resourcePool();
    //filtering discovered resources by discovery mode
    QnResourceList resources;
    for( auto it = resourcesAndSearches.cbegin(); it != resourcesAndSearches.cend(); ++it )
    {
        switch( it->second->discoveryMode() )
        {
            case DiscoveryMode::partiallyEnabled:
                if (auto networkResource = it->first.dynamicCast<QnNetworkResource>())
                {
                    if (!resPool->getNetworkResourceByPhysicalId(networkResource->getPhysicalId()))
                        continue;   //ignoring newly discovered camera
                }
                it->first->addFlags(Qn::search_upd_only);
                break;

            case DiscoveryMode::disabled:
                //discovery totally disabled, ignoring resource
                // Don't mark resource as offline because no search actually was performed.
                continue;

            default:
                break;
        }
        resources.append( std::move(it->first) );
    }
    resourcesAndSearches.clear();

    appendManualDiscoveredResources(resources);
    setLastDiscoveredResources(resources);

    //searching and ignoring auto-discovered cameras already manually added to the resource pool
    for( QnResourceList::iterator it = resources.begin(); it != resources.end(); )
    {
        const QnSecurityCamResource* camRes = dynamic_cast<QnSecurityCamResource*>(it->data());
        if( !camRes || camRes->isManuallyAdded() )
        {
            ++it;
            continue;
        }

        //if camera is already in resource pool and it was added manually, then ignoring it...
        QnNetworkResourcePtr existingRes = resPool->getNetworkResourceByPhysicalId( camRes->getPhysicalId() );
        if( existingRes )
        {

            // Existing camera resource could has empty url in case of Undo camera replacement. At this case allow
            // to find new camera and change 'manual' flag from true to false.
            const QnSecurityCamResource* existingCamRes = dynamic_cast<QnSecurityCamResource*>(existingRes.data());
            if( existingCamRes && existingCamRes->isManuallyAdded()
                && !existingCamRes->getUrl().isEmpty()
                && camRes->getHostAddress() == existingCamRes->getHostAddress())
            {
                NX_VERBOSE(this, "Ignore auto discovered camera %1 because it is added manually", camRes);
                if (isEdgeServer())
                {
                    // Speed optimization for ARM servers.
                    if (existingCamRes->getPhysicalId() == nx::network::getMacFromPrimaryIF())
                    {
                        // On edge server, always discovering camera to be able to move it to the
                        // edge server if needed.
                        ++it;
                        continue;
                    }
                }

                it = resources.erase( it );
                continue;
            }
        }

        ++it;
    }

    {
        NX_MUTEX_LOCKER lock( &m_searchersListMutex );
        if (!m_resourcesToIgnore.empty())
        {
            for (int i = resources.size() - 1; i >= 0; --i)
            {
                if (auto networkResource = resources[i].dynamicCast<QnNetworkResource>())
                {
                    if (m_resourcesToIgnore.contains(networkResource->getPhysicalId()))
                        resources.removeAt(i);
                }
            }
            m_resourcesToIgnore.clear();
        }
    }

    if (processDiscoveredResources(resources, searchType))
    {
        return resources;
    }
    else {
        return QnResourceList();
    }
}

QThreadPool* QnResourceDiscoveryManager::threadPool()
{
    return &m_threadPool;
}

bool QnResourceDiscoveryManager::processDiscoveredResources(QnResourceList& resources, SearchType /*searchType*/)
{
    const auto& resPool = resourcePool();
    //excluding already existing resources
    QnResourceList::iterator it = resources.begin();
    while (it != resources.end())
    {
        if (needToStop())
            return false;

        auto newNetworkResource = (*it).dynamicCast<QnNetworkResource>();
        if (!newNetworkResource)
        {
            ++it; // new resource => should keep it
            continue;
        }

        auto existingResource = resPool->getNetworkResourceByPhysicalId(newNetworkResource->getPhysicalId());
        if (existingResource)
        {
            existingResource->mergeResourcesIfNeeded(newNetworkResource);
            it = resources.erase(it); // Resource is already exists in the pool.

        }
        else
        {
            ++it; // new resource => should keep it
        }
    }

    return !resources.isEmpty();
}

QnManualCameraInfo QnResourceDiscoveryManager::manualCameraInfo(const QnSecurityCamResourcePtr& camera) const
{
    NX_MUTEX_LOCKER lock(&m_searchersListMutex);
    return manualCameraInfoUnsafe(camera);
}

QnManualCameraInfo QnResourceDiscoveryManager::manualCameraInfoUnsafe(const QnSecurityCamResourcePtr& camera) const
{
    const auto resourceTypeId = camera->getTypeId();
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    NX_ASSERT(resourceType, lit("Resource type %1 was not found").arg(resourceTypeId.toString()));

    const auto model = resourceType
        ? resourceType->getName()
        : camera->getModel();
    QnManualCameraInfo info(
        nx::utils::Url(camera->getUrl()), camera->getAuth(), model, camera->getPhysicalId());

    for (const auto& searcher: m_searchersList)
    {
        if (searcher->isResourceTypeSupported(resourceTypeId))
            info.searcher = searcher;
    }

    return info;
}

QSet<QString> QnResourceDiscoveryManager::registerManualCameras(
    const std::vector<QnManualCameraInfo>& cameras,
    Qn::UserSession userSession)
{
    NX_MUTEX_LOCKER lock(&m_searchersListMutex);
    QSet<QString> registeredUniqueIds;
    for (const auto& camera: cameras)
    {
        // This is important to use reverse order of searchers as ONVIF resource type fits both
        // ONVIF and FLEX searchers, while ONVIF is always last one.
        for (auto searcherIterator = m_searchersList.rbegin();
            searcherIterator != m_searchersList.rend();
            ++searcherIterator)
        {
            auto searcher = *searcherIterator;
            if (!camera.resType || !searcher->isResourceTypeSupported(camera.resType->getId()))
                continue;

            NX_DEBUG(this, nx::format("Manual camera %1 is registered for %2 on %3")
                .args(camera.physicalId, typeid(*searcher), camera.url));

            QnManualCameraInfo updatedManualCameraInfo = camera;
            updatedManualCameraInfo.userSession = userSession;
            if (m_manualCameraByUniqueId.contains(camera.physicalId))
                updatedManualCameraInfo.isUpdated = true;

            const auto iterator = m_manualCameraByUniqueId.insert(
                updatedManualCameraInfo.physicalId,
                updatedManualCameraInfo);

            iterator.value().searcher = searcher;
            registeredUniqueIds << camera.physicalId;
            m_manualCameraListChanged = true;
            break;
        }
    }
    return registeredUniqueIds;
}

bool QnResourceDiscoveryManager::isManuallyAdded(const QnSecurityCamResourcePtr& camera) const
{
    if (!camera->isManuallyAdded())
        return false;

    NX_MUTEX_LOCKER lock(&m_searchersListMutex);
    return m_manualCameraByUniqueId.contains(camera->getPhysicalId());
}

QnResourceDiscoveryManager::ResourceSearcherList QnResourceDiscoveryManager::plugins() const
{
    NX_MUTEX_LOCKER lock(&m_searchersListMutex);
    return m_searchersList;
}

QnResourceDiscoveryManager::State QnResourceDiscoveryManager::state() const
{
    return m_state;
}

DiscoveryMode QnResourceDiscoveryManager::discoveryMode() const
{
    if (systemSettings()->isAutoDiscoveryEnabled())
        return DiscoveryMode::fullyEnabled;
    if (isRedundancyUsing())
        return DiscoveryMode::partiallyEnabled;
    return DiscoveryMode::disabled;
}

bool QnResourceDiscoveryManager::isRedundancyUsing() const
{
    auto servers = resourcePool()->servers();
    if (servers.size() < 2)
        return false;

    for (const auto& server: servers)
    {
        if (server->isRedundancy())
            return true;
    }
    return false;
}

void QnResourceDiscoveryManager::updateSearcherUsageUnsafe(QnAbstractResourceSearcher *searcher, bool usePartialEnable)
{
    DiscoveryMode discoveryMode = systemSettings()->isAutoDiscoveryEnabled()
        ? DiscoveryMode::fullyEnabled
        : DiscoveryMode::partiallyEnabled;
    if( searcher->isLocal() ||                  // local resources should always be found
        searcher->isVirtualResource() )         // virtual resources should always be found
    {
        discoveryMode = DiscoveryMode::fullyEnabled;
    }
    else
    {
        QSet<QString> disabledVendorsForAutoSearch;
        // TODO: #akolesnikov Edge server MUST always discover edge camera despite disabledVendors
        //     setting, but MUST check disabledVendors for all other vendors (if they enabled on
        //     edge server).
        if (!isEdgeServer())
            disabledVendorsForAutoSearch = systemSettings()->disabledVendorsSet();

        //no lower_bound, since QSet is built on top of hash
        if( disabledVendorsForAutoSearch.contains(searcher->manufacturer()+lit("=partial")) )
            discoveryMode = DiscoveryMode::partiallyEnabled;
        else if( disabledVendorsForAutoSearch.contains(searcher->manufacturer()) )
            discoveryMode = DiscoveryMode::disabled;
        else if( disabledVendorsForAutoSearch.contains(lit("all=partial")) )
            discoveryMode = DiscoveryMode::partiallyEnabled;
        else if( disabledVendorsForAutoSearch.contains(lit("all")) )
            discoveryMode = DiscoveryMode::disabled;

        if (discoveryMode == DiscoveryMode::partiallyEnabled && !usePartialEnable)
            discoveryMode = DiscoveryMode::disabled;
    }

    searcher->setDiscoveryMode( discoveryMode );
}

void QnResourceDiscoveryManager::updateSearchersUsage()
{
    NX_MUTEX_LOCKER lock(&m_searchersListMutex);

    bool usePartialEnable = isRedundancyUsing();
    for(QnAbstractResourceSearcher *searcher: m_searchersList)
        updateSearcherUsageUnsafe(searcher, usePartialEnable);
}

void QnResourceDiscoveryManager::addResourcesImmediatly(QnResourceList& resources)
{
    processDiscoveredResources(resources, SearchType::Partial);
    m_resourceProcessor->processResources(resources);
}

QnResourceList QnResourceDiscoveryManager::remapPhysicalIdIfNeed(const QnResourceList& resources)
{
    return resources;
}

bool QnResourceDiscoveryManager::isCameraAllowed(
    const QString& /*driverName*/,
    const QnSecurityCamResource* /*securityCamResource*/) const
{
    return true;
}
