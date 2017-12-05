#include "resource_discovery_manager.h"

#include <exception>
#include <list>
#include <set>

#include <QtCore/QElapsedTimer>
#include <QtCore/QThreadPool>

#include <QtConcurrent/QtConcurrent>
#include <QtConcurrent/QtConcurrentMap>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <core/resource/resource.h>
#include <core/resource/abstract_storage_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/resource_searcher.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/storage_plugin_factory.h>

#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <nx/network/ip_range_checker.h>
#include <common/common_module.h>
#include "core/resource/media_server_resource.h"

//deletes trailing slash if present
static QString normalizeUrl(const QString& url)
{
    return nx::utils::Url(url).toString(QUrl::StripTrailingSlash);
}
// ------------------------------------ QnManualCameraInfo -----------------------------

QnManualCameraInfo::QnManualCameraInfo(const nx::utils::Url &url, const QAuthenticator& auth, const QString& resType)
{
    QString urlStr = url.toString();
    this->url = url;
    this->auth = auth;
    this->resType = qnResTypePool->getResourceTypeByName(resType);
    this->searcher = 0;
}

QList<QnResourcePtr> QnManualCameraInfo::checkHostAddr() const
{
    QnAbstractNetworkResourceSearcher* ns = dynamic_cast<QnAbstractNetworkResourceSearcher*>(searcher);
    QString urlStr = url.toString();
    if (ns)
        return ns->checkHostAddr(url, auth, false);
    else
        return QList<QnResourcePtr>();
}



QnResourceDiscoveryManagerTimeoutDelegate::QnResourceDiscoveryManagerTimeoutDelegate( QnResourceDiscoveryManager* discoveryManager )
:
    m_discoveryManager( discoveryManager )
{
}

void QnResourceDiscoveryManagerTimeoutDelegate::onTimeout()
{
    m_discoveryManager->doResourceDiscoverIteration();
}


// ------------------------------------ QnResourceDiscoveryManager -----------------------------

QnResourceDiscoveryManager::QnResourceDiscoveryManager(QObject* parent)
:
    base_type(parent),
    QnCommonModuleAware(parent),
    m_ready( false ),
    m_state( InitialSearch ),
    m_discoveryUpdateIdx(0),
    m_serverOfflineTimeout(QnMediaServerResource::kMinFailoverTimeoutMs)
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnResourceDiscoveryManager::at_resourceDeleted, Qt::DirectConnection);
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnResourceDiscoveryManager::at_resourceAdded, Qt::DirectConnection);
    connect(commonModule()->globalSettings(), &QnGlobalSettings::disabledVendorsChanged, this,
        &QnResourceDiscoveryManager::updateSearchersUsage);
    connect(commonModule()->globalSettings(), &QnGlobalSettings::autoDiscoveryChanged, this,
        &QnResourceDiscoveryManager::updateSearchersUsage);
}

QnResourceDiscoveryManager::~QnResourceDiscoveryManager()
{
    stop();
}

void QnResourceDiscoveryManager::setReady(bool ready)
{
    m_ready = ready;
}

void QnResourceDiscoveryManager::start( Priority priority )
{
    updateSearchersUsage();
    QnLongRunnable::start( priority );
    //moveToThread( this );
}

void QnResourceDiscoveryManager::addDeviceServer(QnAbstractResourceSearcher* serv)
{
    QnMutexLocker locker( &m_searchersListMutex );
    m_searchersList.push_back(serv);
}

void QnResourceDiscoveryManager::addDTSServer(QnAbstractDTSSearcher* serv)
{
    QnMutexLocker locker( &m_searchersListMutex );
    m_dstList.push_back(serv);
}


void QnResourceDiscoveryManager::setResourceProcessor(QnResourceProcessor* processor)
{
    QnMutexLocker locker( &m_searchersListMutex );
    m_resourceProcessor = processor;
}

QnAbstractResourceSearcher* QnResourceDiscoveryManager::searcherByManufacture(
    const QString& manufacture) const
{
    for (const auto& searcher: m_searchersList)
    {
        if (searcher && searcher->manufacture() == manufacture)
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

    if (resourceTypeId == qnResTypePool->getFixedResourceTypeId(QnResourceTypePool::kStorageTypeId))
    {
        result = QnResourcePtr(QnStoragePluginFactory::instance()->createStorage(commonModule(), params.url));
        NX_ASSERT(result); //storage can not be null
    }
    else
    {
        ResourceSearcherList searchersList;
        {
            QnMutexLocker locker( &m_searchersListMutex );
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

void QnResourceDiscoveryManager::pleaseStop()
{
    if (isRunning())
    {
        QnMutexLocker locker( &m_searchersListMutex );
        for (QnAbstractResourceSearcher *searcher: m_searchersList)
            searcher->pleaseStop();
    }

    QnLongRunnable::pleaseStop();

    quit(); //telling thread's event loop to return
}

void QnResourceDiscoveryManager::run()
{
    initSystemThreadId();
    m_runNumber = 0;
    m_timer.reset( new QTimer() );
    m_timer->setSingleShot( true );
    QnResourceDiscoveryManagerTimeoutDelegate timoutDelegate( this );
    connect(m_timer, &QTimer::timeout,
        &timoutDelegate, &QnResourceDiscoveryManagerTimeoutDelegate::onTimeout);
    m_timer->start( 0 );    //immediate execution
    m_state = InitialSearch;

    exec();

    m_timer.reset();
}

static const int GLOBAL_DELAY_BETWEEN_CAMERA_SEARCH_MS = 1000;
static const int MIN_DISCOVERY_SEARCH_MS = 1000 * 15;

void QnResourceDiscoveryManager::doResourceDiscoverIteration()
{
    QElapsedTimer discoveryTime;
    discoveryTime.restart();
    ResourceSearcherList searchersList;
    {
        QnMutexLocker locker( &m_searchersListMutex );
        searchersList = m_searchersList;
    }

    switch( m_state )
    {
        case InitialSearch:
            for (QnAbstractResourceSearcher *searcher: searchersList)
            {
                if ((searcher->discoveryMode() != DiscoveryMode::disabled) && searcher->isLocal())
                {
                    QnResourceList lst = searcher->search();
                    m_resourceProcessor->processResources(lst);
                }
            }
            emit localSearchDone();
            m_state = PeriodicSearch;
            break;

        case PeriodicSearch:
        {
            if( !m_ready )
                break;

            updateLocalNetworkInterfaces();

            if (!m_resourceProcessor->isBusy())
            {
                QnResourceList result = findNewResources();
                if (!result.isEmpty())
                    m_resourceProcessor->processResources(result);
            }

            ++m_runNumber;
            break;
        }
    }

    m_timer->start( qMax(GLOBAL_DELAY_BETWEEN_CAMERA_SEARCH_MS, int(MIN_DISCOVERY_SEARCH_MS - discoveryTime.elapsed()) ));
}

void QnResourceDiscoveryManager::setLastDiscoveredResources(const QnResourceList& resources)
{
    QnMutexLocker lock( &m_resListMutex );
    m_lastDiscoveredResources[m_discoveryUpdateIdx] = resources;
    int sz = sizeof(m_lastDiscoveredResources) / sizeof(QnResourceList);
    m_discoveryUpdateIdx = (m_discoveryUpdateIdx + 1) % sz;
}

QSet<QString> QnResourceDiscoveryManager::lastDiscoveredIds() const
{
    QnMutexLocker lock( &m_resListMutex );
    int sz = sizeof(m_lastDiscoveredResources) / sizeof(QnResourceList);
    QSet<QString> allDiscoveredIds;
    for (int i = 0; i < sz; ++i) {
        for(const QnResourcePtr& res: m_lastDiscoveredResources[i])
            allDiscoveredIds << res->getUniqueId();
    }
    return allDiscoveredIds;
}

void QnResourceDiscoveryManager::updateLocalNetworkInterfaces()
{
    QList<QHostAddress> localAddresses = allLocalIpV4Addresses();
    if (localAddresses != m_allLocalAddresses)
    {
        // Skip first time.
        // We suppose here that allLocalAddresses never returns empty list
        if (!m_allLocalAddresses.isEmpty())
            emit localInterfacesChanged();

        m_allLocalAddresses = localAddresses;
    }
}

static QnResourceList CheckHostAddrAsync(const QnManualCameraInfo& input)
{
    try
    {
        return input.checkHostAddr();
    }
    catch (const std::exception& e)
    {
        qWarning()
            << "CheckHostAddrAsync exception ("<<e.what()<<") caught\n"
            << "\t\tresource type:" << input.resType->getName() << "\n"
            << "\t\tresource url:" << input.url.toString() << "\n";

        return QnResourceList();
    }
    catch (...)
    {
        qWarning()
            << "CheckHostAddrAsync exception caught\n"
            << "\t\tresource type:" << input.resType->getName() << "\n"
            << "\t\tresource url:" << input.url.toString() << "\n";

        return QnResourceList();
    }
}

bool QnResourceDiscoveryManager::canTakeForeignCamera(const QnSecurityCamResourcePtr& camera, int awaitingToMoveCameraCnt)
{
    if (!camera)
        return false;

    if (camera->failoverPriority() == Qn::FP_Never)
        return false;

    QnUuid ownGuid = commonModule()->moduleGUID();
    QnMediaServerResourcePtr mServer = camera->getParentServer();
    const auto& resPool = commonModule()->resourcePool();
    QnMediaServerResourcePtr ownServer = resPool->getResourceById<QnMediaServerResource>(ownGuid);
    if (!mServer)
        return true; // can take foreign camera is camera's server is absent
    if (!ownServer)
        return false;

    if (camera->hasFlags(Qn::desktop_camera))
        return true;

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

    if (camera->preferredServerId() == ownGuid)
        return true;
    else if (mServer->getStatus() == Qn::Online)
        return false;

    if (!ownServer->isRedundancy())
        return false; // redundancy is disabled

    if (resPool->getAllCameras(ownServer, true).size() + awaitingToMoveCameraCnt >= ownServer->getMaxCameras())
        return false;

    return mServer->currentStatusTime() > m_serverOfflineTimeout;
}

void QnResourceDiscoveryManager::appendManualDiscoveredResources(QnResourceList& resources)
{
    decltype(m_manualCameraByUniqueId) manualCameraByUniqueId;
    {
        QnMutexLocker locker(&m_searchersListMutex);
        if (m_manualCameraByUniqueId.empty())
            return;

        manualCameraByUniqueId = m_manualCameraByUniqueId;
    }

    QMap<QString, QnManualCameraInfo> camerasByUrlForSearch;
    for (const auto& manualCamera: manualCameraByUniqueId)
    {
        const auto camera = qSharedPointerDynamicCast<QnSecurityCamResource>(
            commonModule()->resourcePool()->getResourceByUniqueId(manualCamera.uniqueId));

        if (!camera || !camera->hasFlags(Qn::foreigner) || canTakeForeignCamera(camera, 0))
            camerasByUrlForSearch.insert(manualCamera.url.toString(), manualCamera);
    }

    QFuture<QnResourceList> results = QtConcurrent::mapped(camerasByUrlForSearch, &CheckHostAddrAsync);
    results.waitForFinished();
    for (QFuture<QnResourceList>::const_iterator itr = results.constBegin(); itr != results.constEnd(); ++itr)
    {
        QnResourceList foundResources = *itr;
        for (const QnResourcePtr &resource: foundResources)
        {
            if (const QnSecurityCamResourcePtr &camera = resource.dynamicCast<QnSecurityCamResource>())
                camera->setManuallyAdded(true);

            resource->setCommonModule(commonModule());
            resources << resource;
        }
    }
}

QnResourceList QnResourceDiscoveryManager::findNewResources()
{
    std::list<std::pair<QnResourcePtr, QnAbstractResourceSearcher*>> resourcesAndSearches;
    std::set<QString> resourcePhysicalIDs;    //used to detect duplicate resources (same resource found by multiple drivers)
    m_searchersListMutex.lock();
    ResourceSearcherList searchersList = m_searchersList;
    m_searchersListMutex.unlock();

    for (QnAbstractResourceSearcher *searcher: searchersList)
    {
        if ((searcher->discoveryMode() != DiscoveryMode::disabled) && !needToStop())
        {
            QElapsedTimer timer;
            timer.restart();
            QnResourceList lst = searcher->search();
            NX_LOG(lit("Discovery----: searcher %1 took %2 ms to find new resources").
                arg(searcher->manufacture()).
                arg(timer.elapsed()), cl_logDEBUG1);

            // resources can be searched by client in or server.
            // if this client in stand alone => lets add Qn::local
            // server does not care about flags
            for( QnResourceList::iterator
                it = lst.begin();
                it != lst.end();
                 )
            {
                const QnSecurityCamResource* camRes = dynamic_cast<QnSecurityCamResource*>(it->data());
                //checking, if found resource is reserved by some other searcher
                if( camRes &&
                    !CameraDriverRestrictionList::instance()->driverAllowedForCamera( searcher->manufacture(), camRes->getVendor(), camRes->getModel() ) )
                {
                    it = lst.erase( it );
                    continue;   //resource with such unique id is already present
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
    }
    const auto& resPool = commonModule()->resourcePool();
    //filtering discovered resources by discovery mode
    QnResourceList resources;
    for( auto it = resourcesAndSearches.cbegin(); it != resourcesAndSearches.cend(); ++it )
    {
        switch( it->second->discoveryMode() )
        {
            case DiscoveryMode::partiallyEnabled:
                if( !resPool->getResourceByUniqueId(it->first->getUniqueId()) )
                    continue;   //ignoring newly discovered camera
                it->first->addFlags(Qn::search_upd_only);
                break;

            case DiscoveryMode::disabled:
                //discovery totally disabled, ignoring resource
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
    for( QnResourceList::iterator
        it = resources.begin();
        it != resources.end();
         )
    {
        const QnSecurityCamResource* camRes = dynamic_cast<QnSecurityCamResource*>(it->data());
        if( !camRes || camRes->isManuallyAdded() )
        {
            ++it;
            continue;
        }

        //if camera is already in resource pool and it was added manually, then ignoring it...
        QnNetworkResourcePtr existingRes = resPool->getNetResourceByPhysicalId( camRes->getPhysicalId() );
        if( existingRes )
        {
            /* Speed optimization for ARM servers. --ak */
            const QnSecurityCamResource* existingCamRes = dynamic_cast<QnSecurityCamResource*>(existingRes.data());
            if( existingCamRes && existingCamRes->isManuallyAdded() )
            {
#ifdef EDGE_SERVER
                char  mac[MAC_ADDR_LEN];
                char* host = 0;
                getMacFromPrimaryIF(mac, &host);
                if( existingCamRes->getUniqueId().toLocal8Bit() == QByteArray(mac) )
                {
                    //on edge server always discovering camera to be able to move it to the edge server if needed
                    ++it;
                    continue;
                }
#endif

                it = resources.erase( it );
                continue;
            }
        }

        ++it;
    }

    {
        QnMutexLocker lock( &m_searchersListMutex );
        if (!m_recentlyDeleted.empty())
        {
            for (int i = resources.size() - 1; i >= 0; --i)
            {
                if (m_recentlyDeleted.contains(resources[i]->getUniqueId()))
                    resources.removeAt(i);
            }
            m_recentlyDeleted.clear();
        }
    }

    if (processDiscoveredResources(resources, SearchType::Full))
    {
        return resources;
    }
    else {
        return QnResourceList();
    }
}

QnNetworkResourcePtr QnResourceDiscoveryManager::findSameResource(const QnNetworkResourcePtr& netRes)
{
    auto camRes = netRes.dynamicCast<QnVirtualCameraResource>();
    if (!camRes)
        return QnNetworkResourcePtr();

    const auto& resPool = netRes->commonModule()->resourcePool();
    auto existResource = resPool->getResourceByUniqueId<QnVirtualCameraResource>(camRes->getUniqueId());
    if (existResource)
        return existResource;

    for (const auto& existRes: resPool->getResources<QnVirtualCameraResource>())
    {
        bool sameChannels = netRes->getChannel() == existRes->getChannel();
        bool sameMACs = !existRes->getMAC().isNull() && existRes->getMAC() == netRes->getMAC();
        if (sameChannels && sameMACs)
            return existRes;
    }

    return QnNetworkResourcePtr();
}

bool QnResourceDiscoveryManager::processDiscoveredResources(QnResourceList& resources, SearchType /*searchType*/)
{
    const auto& resPool = commonModule()->resourcePool();
    //excluding already existing resources
    QnResourceList::iterator it = resources.begin();
    while (it != resources.end())
    {
        if (needToStop())
            return false;

        const QnResourcePtr& rpResource = resPool->getResourceByUniqueId((*it)->getUniqueId());
        /* Speed optimization for ARM servers. --ak */
        QnNetworkResource* rpNetRes = dynamic_cast<QnNetworkResource*>(rpResource.data());
        if (rpNetRes) {
            QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();
            if (newNetRes)
                rpNetRes->mergeResourcesIfNeeded(newNetRes);
            it = resources.erase(it); // if such res in ResourcePool

        }
        else {
            ++it; // new resource => should keep it
        }
    }

    return !resources.isEmpty();
}

QnManualCameraInfo QnResourceDiscoveryManager::manualCameraInfo(const QnSecurityCamResourcePtr& camera)
{
    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(camera->getTypeId());
    QnManualCameraInfo info(nx::utils::Url(camera->getUrl()), camera->getAuth(), resourceType->getName());
    for (const auto& searcher: m_searchersList)
    {
        if (searcher->isResourceTypeSupported(resourceType->getId()))
            info.searcher = searcher;
    }

    return info;
}

int QnResourceDiscoveryManager::registerManualCameras(const std::vector<QnManualCameraInfo>& cameras)
{
    QnMutexLocker lock(&m_searchersListMutex);
    int addedCount = 0;
    for (const auto& camera: cameras)
    {
        for (const auto& searcher : m_searchersList)
        {
            if (!camera.resType || !searcher->isResourceTypeSupported(camera.resType->getId()))
                continue;

            const auto iterator = m_manualCameraByUniqueId.insert(camera.uniqueId, camera);
            iterator.value().searcher = searcher;
            ++addedCount;
            break;
        }
    }
    return addedCount;
}

void QnResourceDiscoveryManager::at_resourceDeleted(const QnResourcePtr& resource)
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
}

void QnResourceDiscoveryManager::at_resourceAdded(const QnResourcePtr& resource)
{
    const QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (server)
    {
        connect(server, &QnMediaServerResource::redundancyChanged, this, &QnResourceDiscoveryManager::updateSearchersUsage);
        updateSearchersUsage();
    }

    std::vector<QnManualCameraInfo> newCameras;
    {
        QnMutexLocker lock( &m_searchersListMutex );
        const auto camera = resource.dynamicCast<QnSecurityCamResource>();
        if (!camera || !camera->isManuallyAdded())
            return;

        QString cameraNormalUrl = normalizeUrl(camera->getUrl());
        if (!m_manualCameraByUniqueId.contains(camera->getUniqueId()))
        {
            const auto resouceType = qnResTypePool->getResourceType(camera->getTypeId());
            QnManualCameraInfo info(nx::utils::Url(camera->getUrl()), camera->getAuth(), resouceType->getName());
            newCameras.push_back(info);
        }
    }

    if (!newCameras.empty())
        registerManualCameras(newCameras);
}

bool QnResourceDiscoveryManager::isManuallyAdded(const QnSecurityCamResourcePtr& camera) const
{
    if (!camera->isManuallyAdded())
        return false;

    QnMutexLocker lock(&m_searchersListMutex);
    return m_manualCameraByUniqueId.contains(camera->getUniqueId());
}

QnResourceDiscoveryManager::ResourceSearcherList QnResourceDiscoveryManager::plugins() const {
    return m_searchersList;
}

QnResourceDiscoveryManager::State QnResourceDiscoveryManager::state() const
{
    return m_state;
}

bool QnResourceDiscoveryManager::isRedundancyUsing() const
{
    const auto& resPool = commonModule()->resourcePool();
    auto servers = resPool->getAllServers(Qn::AnyStatus);
    if (servers.size() < 2)
        return false;
    for (const auto& server: servers)
    {
        if (server->isRedundancy())
            return true;
    }
    return false;
}

void QnResourceDiscoveryManager::updateSearcherUsage(QnAbstractResourceSearcher *searcher, bool usePartialEnable)
{
    // TODO: #Elric strictly speaking, we must do this under lock.

    DiscoveryMode discoveryMode = commonModule()->globalSettings()->isAutoDiscoveryEnabled() ?
        DiscoveryMode::fullyEnabled :
        DiscoveryMode::partiallyEnabled;
    if( searcher->isLocal() ||                  // local resources should always be found
        searcher->isVirtualResource() )         // virtual resources should always be found
    {
        discoveryMode = DiscoveryMode::fullyEnabled;
    }
    else
    {
        QSet<QString> disabledVendorsForAutoSearch;
        //TODO #ak edge server MUST always discover edge camera despite disabledVendors setting,
        //but MUST check disabledVendors for all other vendors (if they enabled on edge server)
#ifndef EDGE_SERVER
        disabledVendorsForAutoSearch = commonModule()->globalSettings()->disabledVendorsSet();
#endif

        //no lower_bound, since QSet is built on top of hash
        if( disabledVendorsForAutoSearch.contains(searcher->manufacture()+lit("=partial")) )
            discoveryMode = DiscoveryMode::partiallyEnabled;
        else if( disabledVendorsForAutoSearch.contains(searcher->manufacture()) )
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
    ResourceSearcherList searchers;
    {
        QnMutexLocker locker( &m_searchersListMutex );
        searchers = m_searchersList;
    }

    bool usePartialEnable = isRedundancyUsing();
    for(QnAbstractResourceSearcher *searcher: searchers)
        updateSearcherUsage(searcher, usePartialEnable);
}

void QnResourceDiscoveryManager::addResourcesImmediatly(QnResourceList& resources)
{
    for (const auto& resource: resources)
        resource->setCommonModule(commonModule());
    processDiscoveredResources(resources, SearchType::Partial);
    m_resourceProcessor->processResources(resources);
}
