
#include "resource_discovery_manager.h"

#include <QtConcurrent>
#include <set>

#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/QThreadPool>

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

#include <plugins/resource/upnp/upnp_device_searcher.h>
#include <plugins/storage/dts/abstract_dts_searcher.h>

#include <utils/common/sleep.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>
#include <utils/network/ip_range_checker.h>


QnResourceDiscoveryManager* QnResourceDiscoveryManager::m_instance;

// ------------------------------------ QnManualCameraInfo -----------------------------

QnManualCameraInfo::QnManualCameraInfo(const QUrl& url, const QAuthenticator& auth, const QString& resType)
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

QnResourceDiscoveryManager::QnResourceDiscoveryManager()
:
    m_ready( false ),
    m_state( InitialSearch )
{
    connect(QnResourcePool::instance(), SIGNAL(resourceRemoved(const QnResourcePtr&)), this, SLOT(at_resourceDeleted(const QnResourcePtr&)), Qt::DirectConnection);
    connect(QnGlobalSettings::instance(), &QnGlobalSettings::disabledVendorsChanged, this, &QnResourceDiscoveryManager::updateSearchersUsage);
}

QnResourceDiscoveryManager::~QnResourceDiscoveryManager()
{
    stop();
}

void QnResourceDiscoveryManager::setReady(bool ready) 
{
    m_ready = ready;
}

void QnResourceDiscoveryManager::init(QnResourceDiscoveryManager* instance)
{
    m_instance = instance;
}

void QnResourceDiscoveryManager::start( Priority priority )
{
    QnLongRunnable::start( priority );
    //moveToThread( this );
}

QnResourceDiscoveryManager* QnResourceDiscoveryManager::instance()
{
    Q_ASSERT_X(m_instance, "QnResourceDiscoveryManager::init MUST be called first!", Q_FUNC_INFO);
    //return *qnResourceDiscoveryManagerInstance();
    return m_instance;
}

void QnResourceDiscoveryManager::addDeviceServer(QnAbstractResourceSearcher* serv)
{
    QMutexLocker locker(&m_searchersListMutex);
    updateSearcherUsage(serv);
    m_searchersList.push_back(serv);
}

void QnResourceDiscoveryManager::addDTSServer(QnAbstractDTSSearcher* serv)
{
    QMutexLocker locker(&m_searchersListMutex);
    m_dstList.push_back(serv);
}


void QnResourceDiscoveryManager::setResourceProcessor(QnResourceProcessor* processor)
{
    QMutexLocker locker(&m_searchersListMutex);
    m_resourceProcessor = processor;
}

QnResourcePtr QnResourceDiscoveryManager::createResource(const QUuid &resourceTypeId, const QnResourceParams& params)
{
    QnResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return result;

    if (resourceType->getName() == lit("Storage"))
    {
        result = QnResourcePtr(QnStoragePluginFactory::instance()->createStorage(params.url));
    }
    else
    {
        ResourceSearcherList searchersList;
        {
            QMutexLocker locker(&m_searchersListMutex);
            searchersList = m_searchersList;
        }

        foreach (QnAbstractResourceSearcher *searcher, searchersList)
        {
            result = searcher->createResource(resourceTypeId, QnResourceParams(params.url, params.vendor));
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

    quit(); //telling thread's event loop to return
}

void QnResourceDiscoveryManager::run()
{
    initSystemThreadId();
    m_runNumber = 0;
    m_timer.reset( new QTimer() );
    m_timer->setSingleShot( true );
    QnResourceDiscoveryManagerTimeoutDelegate timoutDelegate( this );
    connect( m_timer.get(), SIGNAL(timeout()), &timoutDelegate, SLOT(onTimeout()) );
    m_timer->start( 0 );    //immediate execution
    m_state = InitialSearch;

    exec();

    m_timer.reset();
}

static const int GLOBAL_DELAY_BETWEEN_CAMERA_SEARCH_MS = 1000;

void QnResourceDiscoveryManager::doResourceDiscoverIteration()
{
    if( UPNPDeviceSearcher::instance() )
        UPNPDeviceSearcher::instance()->saveDiscoveredDevicesSnapshot();

    ResourceSearcherList searchersList;
    {
        QMutexLocker locker(&m_searchersListMutex);
        searchersList = m_searchersList;
    }

    switch( m_state )
    {
        case InitialSearch:
            foreach (QnAbstractResourceSearcher *searcher, searchersList)
            {
                if (searcher->shouldBeUsed() && searcher->isLocal())
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

    m_timer->start( GLOBAL_DELAY_BETWEEN_CAMERA_SEARCH_MS );
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

static QnResourceList CheckHostAddrAsync(const QnManualCameraInfo& input) { return input.checkHostAddr(); }

void QnResourceDiscoveryManager::appendManualDiscoveredResources(QnResourceList& resources)
{
    m_searchersListMutex.lock();
    QnManualCameraInfoMap cameras = m_manualCameraMap;
    m_searchersListMutex.unlock();

    QFuture<QnResourceList> results = QtConcurrent::mapped(cameras, &CheckHostAddrAsync);
    results.waitForFinished();
    for (QFuture<QnResourceList>::const_iterator itr = results.constBegin(); itr != results.constEnd(); ++itr)
    {
        QnResourceList foundResources = *itr;
        for (int i = 0; i < foundResources.size(); ++i) {
            QnSecurityCamResourcePtr camera = qSharedPointerDynamicCast<QnSecurityCamResource>(foundResources.at(i));
            if (camera)
                camera->setManuallyAdded(true);
            resources << camera;
        }
    }
}

QnResourceList QnResourceDiscoveryManager::findNewResources()
{
    QnResourceList resources;
    std::set<QString> resourcePhysicalIDs;    //used to detect duplicate resources (same resource found by multiple drivers)
    m_searchersListMutex.lock();
    ResourceSearcherList searchersList = m_searchersList;
    m_searchersListMutex.unlock();

    foreach (QnAbstractResourceSearcher *searcher, searchersList)
    {
        if (searcher->shouldBeUsed() && !needToStop())
        {
            QnResourceList lst = searcher->search();

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

            resources.append(lst);
        }
    }

    appendManualDiscoveredResources(resources);

    {
        QMutexLocker lock(&m_searchersListMutex);
        for (int i = resources.size()-1; i >= 0; --i)
        {
            if (m_recentlyDeleted.contains(resources[i]->getUniqueId()))
                resources.removeAt(i);
        }
        m_recentlyDeleted.clear();
    }

    if (processDiscoveredResources(resources)) 
    {
        dtsAssignment();
        return resources;
    }
    else {
        return QnResourceList();
    }
}

bool QnResourceDiscoveryManager::processDiscoveredResources(QnResourceList& resources)
{
    QMutexLocker lock(&m_discoveryMutex);

    //excluding already existing resources
    QnResourceList::iterator it = resources.begin();
    while (it != resources.end())
    {
        if (needToStop())
            return false;

        const QnResourcePtr& rpResource = qnResPool->getResourceByUniqId((*it)->getUniqueId());
        QnNetworkResource* rpNetRes = dynamic_cast<QnNetworkResource*>(rpResource.data());
        if (rpNetRes) {
            QnNetworkResourcePtr newNetRes = (*it).dynamicCast<QnNetworkResource>();
            if (newNetRes)
                rpNetRes->mergeResourcesIfNeeded(newNetRes);
            it = resources.erase(it); // if such res in ResourcePool

        }
        else {
            ++it; // new resource => shouls keep it
        }
    }

    return !resources.isEmpty();
}

bool QnResourceDiscoveryManager::registerManualCameras(const QnManualCameraInfoMap& cameras)
{
    QMutexLocker lock(&m_searchersListMutex);
    for (QnManualCameraInfoMap::const_iterator itr = cameras.constBegin(); itr != cameras.constEnd(); ++itr)
    {
        for (int i = 0; i < m_searchersList.size(); ++i)
        {
            if (m_searchersList[i]->isResourceTypeSupported(itr.value().resType->getId()))
            {
                QnManualCameraInfoMap::iterator inserted = m_manualCameraMap.insert(itr.key(), itr.value());
                inserted.value().searcher = m_searchersList[i];
            }
        }
    }
    return true;
}


void QnResourceDiscoveryManager::onInitAsyncFinished(const QnResourcePtr& res, bool initialized)
{
    QnNetworkResource* rpNetRes = dynamic_cast<QnNetworkResource*>(res.data());
    if (initialized && rpNetRes)
    {
        if (rpNetRes->getStatus() == Qn::Offline || rpNetRes->getStatus() == Qn::Unauthorized)
            rpNetRes->setStatus(Qn::Online);
    }
}

void QnResourceDiscoveryManager::at_resourceDeleted(const QnResourcePtr& resource)
{
    QMutexLocker lock(&m_searchersListMutex);
    QnManualCameraInfoMap::Iterator itr = m_manualCameraMap.find(resource->getUrl());
    if (itr != m_manualCameraMap.end() && itr.value().resType->getId() == resource->getTypeId())
        m_manualCameraMap.erase(itr);
    m_recentlyDeleted << resource->getUniqueId();
}

bool QnResourceDiscoveryManager::containManualCamera(const QString& uniqId)
{
    QMutexLocker lock(&m_searchersListMutex);
    return m_manualCameraMap.contains(uniqId);
}

QnResourceDiscoveryManager::ResourceSearcherList QnResourceDiscoveryManager::plugins() const {
    return m_searchersList;
}

void QnResourceDiscoveryManager::dtsAssignment()
{
    for (int i = 0; i < m_dstList.size(); ++i)
    {
        //QList<QnDtsUnit> unitsLst =  QnColdStoreDTSSearcher::instance().findDtsUnits();
        QList<QnDtsUnit> unitsLst =  m_dstList[i]->findDtsUnits();

        foreach(QnDtsUnit unit, unitsLst)
        {
            QnResourcePtr res = qnResPool->getResourceByUniqId(unit.resourceID);
            if (!res)
                continue;

            QnVirtualCameraResourcePtr vcRes = qSharedPointerDynamicCast<QnVirtualCameraResource>(res);
            if (!vcRes)
                continue;

            Q_ASSERT(unit.factory!=0);

            vcRes->lockDTSFactory();
            vcRes->setDTSFactory(unit.factory);
            vcRes->unLockDTSFactory();
        }
    }
}

QnResourceDiscoveryManager::State QnResourceDiscoveryManager::state() const 
{ 
    return m_state; 
}

void QnResourceDiscoveryManager::updateSearcherUsage(QnAbstractResourceSearcher *searcher) {
    // TODO: #Elric strictly speaking, we must do this under lock.

    QSet<QString> disabledVendorsForAutoSearch;
#ifndef EDGE_SERVER
    disabledVendorsForAutoSearch = QnGlobalSettings::instance()->disabledVendorsSet();
#endif

    searcher->setShouldBeUsed(
        searcher->isLocal() ||                  // local resources should always be found
        searcher->isVirtualResource() ||        // virtual resources should always be found
        (!disabledVendorsForAutoSearch.contains(searcher->manufacture()) && !disabledVendorsForAutoSearch.contains(lit("all")))
    );
}

void QnResourceDiscoveryManager::updateSearchersUsage() {
    ResourceSearcherList searchers;
    {
        QMutexLocker locker(&m_searchersListMutex);
        searchers = m_searchersList;
    }

    foreach(QnAbstractResourceSearcher *searcher, searchers)
        updateSearcherUsage(searcher);
}
