
#include "manual_camera_searcher.h"

#include <type_traits>

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentMap>

#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/log.h>
#include <utils/network/ip_range_checker.h>

#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_searcher.h>

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>




static const int MAX_PERCENT = 100;
//assuming that scanning ports is ten times faster than plugin check
static const int PORT_SCAN_MAX_PROGRESS_PERCENT = 10;
static_assert( PORT_SCAN_MAX_PROGRESS_PERCENT < MAX_PERCENT, "PORT_SCAN_MAX_PROGRESS_PERCENT must be less than MAX_PERCENT" );

namespace {
    /**
     * @brief resourceExistsInPool          Check if found camera is already exists in pool by its unique ID. For onvif cameras host is also checked.
     * @param resource                      Camera resource.
     * @return                              True if resource is already in resource pool, false otherwise.
     */
    bool resourceExistsInPool(const QnResourcePtr &resource) {
        bool existResource = false;
        QnResourcePtr res = qnResPool->getResourceByUniqueId(resource->getUniqueId());
        if (res && qnResPool->getResourceById(res->getParentId())) {
            existResource = true; // already in resource pool
        }
        else {
            // For onvif uniqID may be different. Some GUID in pool and macAddress after manual adding. So, do addition checking for IP address
            QnSecurityCamResourcePtr netRes = resource.dynamicCast<QnSecurityCamResource>();
            if (netRes) {
                QnNetworkResourceList existResList = qnResPool->getAllNetResourceByHostAddress(netRes->getHostAddress());
                existResList = existResList.filtered(([](const QnNetworkResourcePtr& res) { return qnResPool->getResourceById(res->getParentId());} ));
                for(const QnNetworkResourcePtr& existRes: existResList) 
                {
                    QnVirtualCameraResourcePtr existCam = existRes.dynamicCast<QnVirtualCameraResource>();
                    if (!existCam)
                        continue;

                    bool newIsRtsp = (netRes->getVendor() == lit("GENERIC_RTSP"));  //TODO #ak remove this!
                    bool existIsRtsp = (existCam->getVendor() == lit("GENERIC_RTSP"));  //TODO #ak remove this!
                    if (newIsRtsp && !existIsRtsp)
                        continue; // allow to stack RTSP and non RTSP cameras with same IP:port

                    if (!existCam->isManuallyAdded())
                        existResource = true; // block manual and auto add in same time
                    else if (existRes->getTypeId() != netRes->getTypeId()) 
                    {
                        // allow several manual cameras on the same IP if cameras have different ports
                        QUrl url1(existRes->getUrl());
                        QUrl url2(netRes->getUrl());
                        if (url1.port() == url2.port())
                            existResource = true; // camera found by different drivers on the same port
                    }
                }
            }
        }
        return existResource;
    }

    QnManualCameraSearchSingleCamera fromResource(const QnResourcePtr &resource) 
    {
        QnSecurityCamResourcePtr cameraResource = resource.dynamicCast<QnSecurityCamResource>();
        return QnManualCameraSearchSingleCamera(resource->getName(),
                                                resource->getUrl(),
                                                qnResTypePool->getResourceType(resource->getTypeId())->getName(),
                                                cameraResource->getVendor(),
                                                cameraResource->getUniqueId(),
                                                resourceExistsInPool(resource));
    }

    QList<QnAbstractNetworkResourceSearcher*> getAllNetworkSearchers()
    {
        QList<QnAbstractNetworkResourceSearcher*> result;

        for(QnAbstractResourceSearcher* as: QnResourceDiscoveryManager::instance()->plugins()) 
        {
            QnAbstractNetworkResourceSearcher* ns = 
                dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
            Q_ASSERT(ns);

            result.push_back(ns);
        }

        return result;
    }

    
}

struct SinglePluginChecker 
{
    SinglePluginChecker(
        QnAbstractNetworkResourceSearcher* plugin, const QUrl &url, const QAuthenticator &auth):
        plugin(plugin), url(url), auth(auth) 
    {
    }

    QnManualCameraSearchCameraList mapFunction() const
    {
        QnManualCameraSearchCameraList results;
        for(const QnResourcePtr &resource: plugin->checkHostAddr(url, auth, true))
        {
            QnSecurityCamResourcePtr camRes = resource.dynamicCast<QnSecurityCamResource>();

            //checking, if found resource is reserved by some other searcher
            if( camRes && !CameraDriverRestrictionList::instance()->driverAllowedForCamera(
                    plugin->manufacture(), 
                    camRes->getVendor(), 
                    camRes->getModel() ))
            {
                continue;   //camera is not allowed to be used with this driver
            }

            results << fromResource(resource);
        }

        return results;
    }

    QnAbstractNetworkResourceSearcher* plugin;
    QUrl url;
    QAuthenticator auth;
};

struct PluginsEnumerator 
{
    PluginsEnumerator(const QString &addr, int port, const QAuthenticator &auth):
        auth(auth)
    {
        if(QUrl(addr).scheme().isEmpty())
            url.setHost(addr);
        else
            url.setUrl(addr);

        if (port)
            url.setPort(port);
    }

    QList<SinglePluginChecker> enumerate() const 
    {
        QList<SinglePluginChecker> result;
        for(QnAbstractResourceSearcher* as: QnResourceDiscoveryManager::instance()->plugins()) 
        {
            QnAbstractNetworkResourceSearcher* ns = dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
            Q_ASSERT( ns );
            result << SinglePluginChecker(ns, url, auth);
        }
        return result;
    }

    static int pluginsCount() 
    {
        return QnResourceDiscoveryManager::instance()->plugins().size();
    }

    QUrl url;
    QAuthenticator auth;
};

QnSearchTask::QnSearchTask(
    const QString& addr, 
    int port, 
    const QAuthenticator& auth, 
    bool breakOnGotResult):

    m_auth(auth),
    m_breakIfGotResult(breakOnGotResult),
    m_blocking(false),
    m_interruptTaskProcesing(false)
{
    if(QUrl(addr).scheme().isEmpty())
        m_url.setHost(addr);
    else
        m_url.setUrl(addr);

    if (port)
        m_url.setPort(port);
}

void QnSearchTask::setSearchers(const SearcherList& searchers)
{
    m_searchers = searchers;
}

void QnSearchTask::setSearchDoneCallback(const SearchDoneCallback& callback)
{
    m_callback = callback;
}

void QnSearchTask::setBlocking(bool isBlocking)
{
    m_blocking = isBlocking;
}

void QnSearchTask::setInterruptTaskProcessing(bool interrupt)
{
    m_interruptTaskProcesing = interrupt;
}

bool QnSearchTask::isBlocking() const
{
    return m_blocking;
}

bool QnSearchTask::doesInterruptTaskProcessing() const
{
    return m_interruptTaskProcesing;
}

void QnSearchTask::doSearch()
{
    QnManualCameraSearchCameraList results;
    for (const auto& checker: m_searchers)
    {
        auto seqResults = checker->checkHostAddr(m_url, m_auth, true);

        for (const auto& res: seqResults)
        {
            QnSecurityCamResourcePtr camRes = res.dynamicCast<QnSecurityCamResource>();

            // Checking, if found resource is reserved by some other searcher
            if(camRes && !CameraDriverRestrictionList::instance()->driverAllowedForCamera(
                checker->manufacture(), 
                camRes->getVendor(), 
                camRes->getModel()))
            {
                continue;   //< Camera is not allowed to be used with this driver.
            }

            results.push_back(fromResource(res));
        }

        if (!results.isEmpty() && m_breakIfGotResult)
        {
            m_callback(results, this);
            return;
        }
    }

    m_callback(results, this);
}

QUrl QnSearchTask::url()
{
    return m_url;
}

QnManualCameraSearcher::QnManualCameraSearcher():
    m_state(QnManualCameraSearchStatus::Init),
    m_scanProgress(NULL),
    m_cancelled(false),
    m_hostRangeSize(0)
{
}

QnManualCameraSearcher::~QnManualCameraSearcher() 
{
}

bool QnManualCameraSearcher::run(
    QThreadPool* threadPool, 
    const QString& startAddr, 
    const QString& endAddr, 
    const QAuthenticator& auth, 
    int port ) 
{
    qDebug() << "Running MANUAL SEARCHER";
    {
        QnMutexLocker lock( &m_mutex );
        if (m_state == QnManualCameraSearchStatus::Aborted)
            return false;
    }

    m_totalTaskCount = 0;
    
    QnSearchTask::SearchDoneCallback callback = 
        [this, threadPool](const QnManualCameraSearchCameraList& results, QnSearchTask* const task)
        {
            QnMutexLocker lock(&m_mutex);

            auto queueName = task->url().toString();

            m_totalTaskCount--;
            m_runningTaskCount[queueName]--;

            if (m_runningTaskCount[queueName] > 0)
                return;

            m_results.append(results);
            QList<QnSearchTask> taskToRun;

            if (task->doesInterruptTaskProcessing() && !results.isEmpty())
            {
                while (!m_urlSearchTaskQueues[queueName].isEmpty())
                {
                    m_urlSearchTaskQueues[queueName].dequeue();
                    m_totalTaskCount--;
                }

                if (m_totalTaskCount == 0)
                    m_waitCondition.wakeOne();

                return;
            }

            runTasks(threadPool, queueName);

            if (m_totalTaskCount == 0)
                m_waitCondition.wakeOne();
        };

    QnSearchTask::SearcherList sequentialSearchers;
    QnSearchTask::SearcherList parallelSearchers;

    auto searchers = getAllNetworkSearchers();

    for (const auto& searcher: searchers)
    {
        if (searcher->isSequential())
            sequentialSearchers.push_back(searcher);
        else
            parallelSearchers.push_back(searcher);
    }

    QStringList onlineHosts;
    m_singleAddressCheck = endAddr.isNull();

    if (m_singleAddressCheck) 
        onlineHosts.push_back(startAddr);
    else 
        onlineHosts = getOnlineHosts(startAddr, endAddr, port);

    for (const auto& host: onlineHosts)
    {
        QString hostToCheck;

        QnSearchTask sequentialTask(host, port, auth, true);
        sequentialTask.setSearchers(sequentialSearchers);
        sequentialTask.setSearchDoneCallback(callback);
        sequentialTask.setBlocking(true);
        sequentialTask.setInterruptTaskProcessing(true);

        hostToCheck = sequentialTask.url().toString();

        m_urlSearchTaskQueues[hostToCheck].enqueue(sequentialTask);

        m_totalTaskCount++;

        for (const auto& searcher: parallelSearchers)
        {
            QnSearchTask::SearcherList searcherList;
            searcherList.push_back(searcher);

            QnSearchTask parallelTask(host, port, auth, false);
            parallelTask.setSearchers(searcherList);
            parallelTask.setSearchDoneCallback(callback);

            m_urlSearchTaskQueues[hostToCheck].enqueue(parallelTask);

            m_totalTaskCount++;
        }
    }

    m_state = QnManualCameraSearchStatus::CheckingHost;
   
    for (const auto& queueName: m_urlSearchTaskQueues.keys())
    {
        qDebug() << "Running tasks from queue queueName";
        runTasks(threadPool, queueName);
    }

    int kMaxWaitTime = 2000;

    QnMutex localMutex;
    QnMutexLocker lock2(&localMutex);
    while(m_totalTaskCount > 0)
        m_waitCondition.wait(&localMutex, kMaxWaitTime);

    {
        QnMutexLocker lock( &m_mutex );
        /*for (size_t i = 0; i < results.resultCount(); ++i) 
        {
            if (!results.isResultReadyAt(i))
                continue;

            m_results << results.resultAt(i);
        }*/

        m_scanProgress = NULL;

        m_state = QnManualCameraSearchStatus::Finished;

        /*if (results.isCanceled())
            m_state = QnManualCameraSearchStatus::Aborted;
        else
            m_state = QnManualCameraSearchStatus::Finished;*/
    }

    /*{
        QnConcurrent::QnFuture<QnManualCameraSearchCameraList> results;

        {
            QnMutexLocker lock( &m_mutex );

            if (m_state == QnManualCameraSearchStatus::Aborted)
                return false;

            m_state = QnManualCameraSearchStatus::CheckingHost;
            QnScopedThreadRollback ensureFreeThread(1);

            results = QnConcurrent::mapped(
                threadPool,
                checkers,
                std::mem_fn(&SinglePluginChecker::mapFunction));

            m_scanProgress = &results;
        }

        results.waitForFinished();

        
    }*/

    return true;
}

void QnManualCameraSearcher::cancel() {
    QnMutexLocker lock( &m_mutex );
    m_cancelled = true;
    switch(m_state) {
    case QnManualCameraSearchStatus::Finished:
        return;
    case QnManualCameraSearchStatus::CheckingOnline:
        m_ipChecker.pleaseStop();
        m_ipChecker.join();
        break;
    case QnManualCameraSearchStatus::CheckingHost:
        if (!m_scanProgress)
            return;
        m_scanProgress->cancel();
        m_scanProgress->waitForFinished();
        break;
    default:
        break;
    }
}

QnManualCameraSearchProcessStatus QnManualCameraSearcher::status() const {
    QnMutexLocker lock( &m_mutex );

    QnManualCameraSearchProcessStatus result;

    // filling cameras field
    switch(m_state) {
    case QnManualCameraSearchStatus::CheckingHost:
        if (m_scanProgress) {
            for (size_t i = 0; i < m_scanProgress->resultCount(); ++i) {
                if (!m_scanProgress->isResultReadyAt(i))
                    continue;
                result.cameras << m_scanProgress->resultAt(i);
            }
        }
        break;
    case QnManualCameraSearchStatus::Finished:
    case QnManualCameraSearchStatus::Aborted:
        result.cameras = m_results;
        break;
    default:
        break;
    }

    // filling state field
    switch(m_state) {
    case QnManualCameraSearchStatus::Finished:
    case QnManualCameraSearchStatus::Aborted:
        result.status = QnManualCameraSearchStatus(m_state, MAX_PERCENT, MAX_PERCENT);
        NX_LOG( lit(" -----------------1 %1 : %2").arg(result.status.current).arg(result.status.total), cl_logDEBUG1 );
        break;
    case QnManualCameraSearchStatus::CheckingOnline:
        //considering it to be half of entire job
        result.status = QnManualCameraSearchStatus( m_state, (int)m_ipChecker.hostsChecked()*PORT_SCAN_MAX_PROGRESS_PERCENT/m_hostRangeSize, MAX_PERCENT );
        NX_LOG( lit(" -----------------2 %1 : %2").arg(result.status.current).arg(result.status.total), cl_logDEBUG1 );
        break;
    case QnManualCameraSearchStatus::CheckingHost:
    {
        if (!m_scanProgress) {
            result.status = QnManualCameraSearchStatus(m_state, PORT_SCAN_MAX_PROGRESS_PERCENT, MAX_PERCENT);
            NX_LOG( lit(" -----------------3 %1 : %2").arg(result.status.current).arg(result.status.total), cl_logDEBUG1 );
        } else {
            const size_t maxProgress = m_scanProgress->progressMaximum() - m_scanProgress->progressMinimum();
            Q_ASSERT( m_scanProgress->progressMaximum() >= m_scanProgress->progressMinimum() );
            const size_t currentProgress = m_scanProgress->progressValue();
            //considering it to be second half of entire job
            result.status = QnManualCameraSearchStatus(
                m_state,
                PORT_SCAN_MAX_PROGRESS_PERCENT + currentProgress*(MAX_PERCENT - PORT_SCAN_MAX_PROGRESS_PERCENT)/(maxProgress > 0 ? maxProgress : currentProgress),
                MAX_PERCENT );
            NX_LOG( lit(" -----------------4 %1 : %2").arg(result.status.current).arg(result.status.total), cl_logDEBUG1 );
        }
        break;
    }
    default:
        result.status = QnManualCameraSearchStatus(m_state, 0, MAX_PERCENT);
        break;
    }
    return result;
}

QStringList QnManualCameraSearcher::getOnlineHosts(
    const QString& startAddr, 
    const QString& endAddr,
    int port)
{
    QStringList onlineHosts;

    const quint32 startIPv4Addr = QHostAddress(startAddr).toIPv4Address();
    const quint32 endIPv4Addr = QHostAddress(endAddr).toIPv4Address();

    if (endIPv4Addr < startIPv4Addr)
        return QStringList();

    {
        QnMutexLocker lock( &m_mutex );
        m_hostRangeSize = endIPv4Addr - startIPv4Addr;
        m_state = QnManualCameraSearchStatus::CheckingOnline;
    }

    onlineHosts = m_ipChecker.onlineHosts(
        QHostAddress(startAddr),
        QHostAddress(endAddr),
        port ? port : nx_http::DEFAULT_HTTP_PORT );

    {
        QnMutexLocker lock( &m_mutex );
        if( m_cancelled )
        {
            m_state = QnManualCameraSearchStatus::Aborted;
            return QStringList();
        }
    }

    return onlineHosts;
}

void QnManualCameraSearcher::runTasks(QThreadPool* threadPool, const QString& queueName)
{
    Q_ASSERT(m_urlSearchTaskQueues.contains(queueName));

    if (m_runningTaskCount[queueName] > 0)
        return;

    auto& queue = m_urlSearchTaskQueues[queueName];

    while(!queue.isEmpty())
    {
        QnSearchTask task = queue.dequeue();
        auto taskFn = std::bind(&QnSearchTask::doSearch, task);

        QnConcurrent::run(threadPool, taskFn);

        m_runningTaskCount[queueName]++;

        if (task.isBlocking())
            break;
    }
}
