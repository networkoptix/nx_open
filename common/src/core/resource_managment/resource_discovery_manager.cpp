
#include <set>

#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/QThreadPool>
#include "resource_discovery_manager.h"
#include "utils/common/sleep.h"
#include "resource_searcher.h"
#include "../resource/network_resource.h"
#include "resource_pool.h"
#include "utils/common/util.h"
#include "api/app_server_connection.h"
#include "utils/common/synctime.h"
#include "utils/network/ping.h"
#include "utils/network/ip_range_checker.h"
#include "plugins/resources/upnp/upnp_device_searcher.h"
#include "plugins/storage/dts/abstract_dts_searcher.h"
#include "core/resource/abstract_storage_resource.h"
#include "core/resource/storage_resource.h"
#include "camera_driver_restriction_list.h"


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

QnResourceDiscoveryManager::QnResourceDiscoveryManager( const CameraDriverRestrictionList* cameraDriverRestrictionList )
:
    m_ready( false ),
    m_state( initialSearch ),
    m_cameraDriverRestrictionList( cameraDriverRestrictionList )
{
    connect(QnResourcePool::instance(), SIGNAL(resourceRemoved(const QnResourcePtr&)), this, SLOT(at_resourceDeleted(const QnResourcePtr&)), Qt::DirectConnection);
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
    serv->setShouldBeUsed(!m_disabledVendorsForAutoSearch.contains(serv->manufacture()));
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

QnResourcePtr QnResourceDiscoveryManager::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
        return result;
    if (resourceType->getName() == QLatin1String("Storage"))
    {

        result = QnResourcePtr(QnStoragePluginFactory::instance()->createStorage(parameters[QLatin1String("url")]));
        if (result)
            result->deserialize(parameters);
    }
    else {
        ResourceSearcherList searchersList;
        {
            QMutexLocker locker(&m_searchersListMutex);
            searchersList = m_searchersList;
        }

        int i = 0;
        foreach (QnAbstractResourceSearcher *searcher, searchersList)
        {
            result = searcher->createResource(resourceTypeId, parameters);
            if (!result.isNull())
                break;
            i++;
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
    saveSysThreadID();
    m_runNumber = 0;
    m_timer.reset( new QTimer() );
    m_timer->setSingleShot( true );
    QnResourceDiscoveryManagerTimeoutDelegate timoutDelegate( this );
    connect( m_timer.get(), SIGNAL(timeout()), &timoutDelegate, SLOT(onTimeout()) );
    m_timer->start( 0 );    //immediate execution
    m_state = initialSearch;

    exec();

    m_timer.reset();
}

static int GLOBAL_DELAY_BETWEEN_CAMERA_SEARCH_MS = 1000;

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
        case initialSearch:
            foreach (QnAbstractResourceSearcher *searcher, searchersList)
            {
                if (searcher->shouldBeUsed() && dynamic_cast<QnAbstractFileResourceSearcher*>(searcher))
                {
                    QnResourceList lst = searcher->search();
                    m_resourceProcessor->processResources(lst);
                }
            }
            m_state = periodicSearch;
            break;

        case periodicSearch:
        {
            if( !m_ready )
                break;

            updateLocalNetworkInterfaces();

            QnResourceList result = findNewResources();

            if (!result.isEmpty())
            {
                m_resourceProcessor->processResources(result);
            }

            int global_delay_between_search = 1000;
            smartSleep(global_delay_between_search);
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

void QnResourceDiscoveryManager::appendManualDiscoveredResources(QnResourceList& resources)
{
    m_searchersListMutex.lock();
    QnManualCamerasMap cameras = m_manualCameraMap;
    m_searchersListMutex.unlock();

    for (QnManualCamerasMap::const_iterator itr = cameras.constBegin(); itr != cameras.constEnd(); ++itr)
    {
        QList<QnResourcePtr> foundResources = itr.value().checkHostAddr();
        for (int i = 0; i < foundResources.size(); ++i) {
            QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(foundResources.at(i));
            if (camera)
                camera->setManuallyAdded(true);
            resources << camera;
        }
    }
}

QnResourceList QnResourceDiscoveryManager::findNewResources()
{
    QTime time;
    time.start();

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
            // if this client in stand alone => lets add QnResource::local 
            // server does not care about flags 
            for( QnResourceList::iterator
                it = lst.begin();
                it != lst.end();
                 )
            {
                QnVirtualCameraResourcePtr virtCamRes = it->dynamicCast<QnVirtualCameraResource>();
                //checking, if found resource is reserved by some other searcher
                if( virtCamRes &&
                    m_cameraDriverRestrictionList &&
                    !m_cameraDriverRestrictionList->driverAllowedForCamera( searcher->manufacture(), virtCamRes->getVendorName(), virtCamRes->getModel() ) )
                {
                    it = lst.erase( it );
                    continue;   //resource with such unique id is already present
                }

                QnNetworkResourcePtr networkRes = it->dynamicCast<QnNetworkResource>();
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
                    (*it)->addFlags( QnResource::local );

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
        //cl_log.log("Discovery---- Done. Time elapsed: ", time.elapsed(), cl_logDEBUG1);
        return resources;
    }
    else {
        return QnResourceList();
    }
}

/*
QnResourceList QnResourceDiscoveryManager::processManualAddedResources()
{
    QnResourceList resources;
    appendManualDiscoveredResources(resources);
    if (processDiscoveredResources(resources))
        return resources;
    else
        return QnResourceList();
}
*/

bool QnResourceDiscoveryManager::processDiscoveredResources(QnResourceList& resources)
{
    QMutexLocker lock(&m_discoveryMutex);

    //excluding already existing resources
    QnResourceList::iterator it = resources.begin();
    while (it != resources.end())
    {
        if (needToStop())
            return false;

        QnResourcePtr rpResource = qnResPool->getResourceByUniqId((*it)->getUniqueId());
        QnNetworkResourcePtr rpNetRes = rpResource.dynamicCast<QnNetworkResource>();
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

struct ManualSearcherHelper
{
    ManualSearcherHelper(): plugins(0) {}

    QUrl url;
    QnResourceDiscoveryManager::ResourceSearcherList* plugins;
    QList<QnResourcePtr> resList;
    QAuthenticator auth;

    void f()
    {
        foreach(QnAbstractResourceSearcher* as, *plugins)
        {
            QnAbstractNetworkResourceSearcher* ns = dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
            Q_ASSERT( ns );
            resList = ns->checkHostAddr(url, auth, true);
            if (!resList.isEmpty())
                break;
        }
    }
};


QnResourceList QnResourceDiscoveryManager::findResources(QString startAddr, QString endAddr, const QAuthenticator& auth, int port)
{
    
    {
        QString str;
        QTextStream stream(&str);

        stream << "Looking for cameras... StartAddr = " << startAddr << "  EndAddr = " << endAddr << "   login/pass = " << auth.user() << "/" << auth.password();
        cl_log.log(str, cl_logINFO);
    }

    //=======================================
    QnIprangeChecker ip_cheker;

    cl_log.log("Checking for online addresses....", cl_logINFO);

    QList<QString> online;
    if (endAddr.isNull())
        online << startAddr;
    else
        online = ip_cheker.onlineHosts(QHostAddress(startAddr), QHostAddress(endAddr), port ? port : 80);


    cl_log.log("Found ", online.size(), " IPs:", cl_logINFO);

    foreach(const QString& addr, online)
    {
        cl_log.log(addr, cl_logINFO);
    }

    //=======================================

    //now lets check each one of online machines...

    QList<ManualSearcherHelper> testList;
    foreach(const QString& addr, online)
    {
        ManualSearcherHelper t;
        if( QUrl(addr).scheme().isEmpty() )
            t.url.setHost(addr);
        else
            t.url.setUrl(addr);
        if (port)
            t.url.setPort(port);
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
        result.append(h.resList);
        /*
        for (int i = 0; i < h.resList.size(); ++i)
        {
            if (qnResPool->hasSuchResource(h.resList[i]->getUniqueId())) // already in resource pool 
                continue;

            // For onvif uniqID may be different. Some GUID in pool and macAddress after manual adding. So, do addition cheking for IP address
            QnResourcePtr existRes = qnResPool->getResourceByUrl(h.url.host());
            if (existRes && (existRes->getStatus() == QnResource::Online || existRes->getStatus() == QnResource::Recording))
                continue;

            result.push_back(h.resList[i]);
        }
        */
    }

    cl_log.log("Found ",  result.size(), " new resources", cl_logINFO);
    foreach(QnResourcePtr res, result)
    {
        cl_log.log(res->toString(), cl_logINFO);
    }


    return result;
}

bool QnResourceDiscoveryManager::registerManualCameras(const QnManualCamerasMap& cameras)
{
    QMutexLocker lock(&m_searchersListMutex);
    for (QnManualCamerasMap::const_iterator itr = cameras.constBegin(); itr != cameras.constEnd(); ++itr)
    {
        for (int i = 0; i < m_searchersList.size(); ++i)
        {
            if (m_searchersList[i]->isResourceTypeSupported(itr.value().resType->getId()))
            {
                QnManualCamerasMap::iterator inserted = m_manualCameraMap.insert(itr.key(), itr.value());
                inserted.value().searcher = m_searchersList[i];
            }
        }
    }
    return true;
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

void QnResourceDiscoveryManager::at_resourceDeleted(const QnResourcePtr& resource)
{
    QMutexLocker lock(&m_searchersListMutex);
    QnManualCamerasMap::Iterator itr = m_manualCameraMap.find(resource->getUrl());
    if (itr != m_manualCameraMap.end() && itr.value().resType->getId() == resource->getTypeId())
        m_manualCameraMap.erase(itr);
    m_recentlyDeleted << resource->getUniqueId();
}

bool QnResourceDiscoveryManager::containManualCamera(const QString& uniqId)
{
    QMutexLocker lock(&m_searchersListMutex);
    return m_manualCameraMap.contains(uniqId);
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

void QnResourceDiscoveryManager::setDisabledVendors(const QStringList& vendors)
{
    m_disabledVendorsForAutoSearch = vendors.toSet();
}

