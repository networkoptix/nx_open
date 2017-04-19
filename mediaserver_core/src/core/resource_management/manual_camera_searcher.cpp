
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
    bool resourceExistsInPool(const QnResourcePtr &resource) 
    {
        bool existResource = false;
        QnResourcePtr res = qnResPool->getResourceByUniqueId(resource->getUniqueId());
        if (res && qnResPool->getResourceById(res->getParentId())) 
        {
            existResource = true; // already in resource pool
        }
        else 
        {
            // For onvif uniqID may be different. Some GUID in pool and macAddress after manual adding. So, do addition checking for IP address
            QnSecurityCamResourcePtr netRes = resource.dynamicCast<QnSecurityCamResource>();
            if (netRes) 
            {
                QnNetworkResourceList existResList = qnResPool->getAllNetResourceByHostAddress(netRes->getHostAddress());
                existResList = existResList.filtered(
                    [](const QnNetworkResourcePtr& res) 
                    { 
                        bool hasParent = qnResPool->getResourceById(res->getParentId());
                        return hasParent && res->getStatus() != Qn::Offline;
                    });

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
                    {
                        existResource = true; // block manual and auto add in same time
                    }
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
        return QnManualCameraSearchSingleCamera(
            resource->getName(),
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

QString QnSearchTask::toString()
{
    QString str;

    for (const auto& searcher: m_searchers)
        str += searcher->manufacture() + lit(" ");

    return str;
}

QnManualCameraSearcher::QnManualCameraSearcher():
    m_state(QnManualCameraSearchStatus::Init),
    m_cancelled(false),
    m_hostRangeSize(0),
    m_totalTaskCount(0),
    m_remainingTaskCount(0)
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

    {
        QnMutexLocker lock( &m_mutex );
        if (m_state == QnManualCameraSearchStatus::Aborted)
            return false;

        m_totalTaskCount = 0;
        m_remainingTaskCount = 0;
        m_urlSearchTaskQueues.clear();
        m_searchQueueContexts.clear();
        m_cancelled = false;
    }
    
    QnSearchTask::SearchDoneCallback callback = 
        [this, threadPool](const QnManualCameraSearchCameraList& results, QnSearchTask* const task)
        {
            QnMutexLocker lock(&m_mutex);
            auto queueName = task->url().toString();
            
            m_remainingTaskCount--;

            auto& context = m_searchQueueContexts[queueName];

            context.isBlocked = false;
            context.runningTaskCount--;

            if (m_cancelled)
            {
                m_waitCondition.wakeOne();
                return;
            }

            m_results.append(results);

            if (task->doesInterruptTaskProcessing() && !results.isEmpty())
            {
                m_remainingTaskCount -= m_urlSearchTaskQueues[queueName].size();
                context.isInterrupted = true;
            }

            runTasksUnsafe(threadPool);

            if (m_remainingTaskCount == 0 || m_cancelled)
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

    if (endAddr.isNull())
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
        m_searchQueueContexts[hostToCheck] = SearchTaskQueueContext();

        m_remainingTaskCount++;
        
        for (const auto& searcher: parallelSearchers)
        {
            QnSearchTask::SearcherList searcherList;
            searcherList.push_back(searcher);

            QnSearchTask parallelTask(host, port, auth, false);
            parallelTask.setSearchers(searcherList);
            parallelTask.setSearchDoneCallback(callback);

            m_urlSearchTaskQueues[hostToCheck].enqueue(parallelTask);

            m_remainingTaskCount++;
        }
    }

    m_totalTaskCount = m_remainingTaskCount;
    int kMaxWaitTimeMs = 2000;

    auto hasRunningTasks = 
        [this]() -> bool
        {
            for (const auto& context: m_searchQueueContexts)
            {
                if (context.isInterrupted)
                    continue;

                if (context.runningTaskCount > 0)
                    return true;
            }

            return false;
        };

    
    QnMutexLocker lock(&m_mutex);
    m_state = QnManualCameraSearchStatus::CheckingHost;
    if (m_cancelled)
    {
        m_state = QnManualCameraSearchStatus::Aborted;
        return true;
    }

    runTasksUnsafe(threadPool);

    while((m_remainingTaskCount > 0 && !m_cancelled) || hasRunningTasks())
        m_waitCondition.wait(&m_mutex, kMaxWaitTimeMs);

    if (!m_cancelled)
        m_state = QnManualCameraSearchStatus::Finished;
    
    return true;
}

void QnManualCameraSearcher::cancel() {

    QnMutexLocker lock( &m_mutex );
    m_cancelled = true;

    switch (m_state)
    {
        case QnManualCameraSearchStatus::Finished:
        {
            return;
        }
        case QnManualCameraSearchStatus::CheckingOnline:
        {
            m_ipChecker.pleaseStop();
            m_ipChecker.join();
            break;
        }
        default:
        {
            break;
        }
    }

    m_state = QnManualCameraSearchStatus::Aborted;
}

QnManualCameraSearchProcessStatus QnManualCameraSearcher::status() const 
{
    QnMutexLocker lock( &m_mutex );
    QnManualCameraSearchProcessStatus result;

    switch (m_state) 
    {
        case QnManualCameraSearchStatus::CheckingHost:
        {
            result.cameras = m_results;
            break;
        }
        case QnManualCameraSearchStatus::Finished:
        case QnManualCameraSearchStatus::Aborted:
        {
            result.cameras = m_results;
            break;
        }
        default:
        {
            break;
        }
    }

    switch (m_state) 
    {
        case QnManualCameraSearchStatus::Finished:
        case QnManualCameraSearchStatus::Aborted:
        {
            result.status = QnManualCameraSearchStatus(m_state, MAX_PERCENT, MAX_PERCENT);
            NX_LOG(lit(" -----------------1 %1 : %2")
                .arg(result.status.current)
                .arg(result.status.total), 
                cl_logDEBUG1 );
            break;
        }
        case QnManualCameraSearchStatus::CheckingOnline:
        {
            Q_ASSERT(m_hostRangeSize);
            int currentProgress = m_hostRangeSize ?
                m_ipChecker.hostsChecked() * PORT_SCAN_MAX_PROGRESS_PERCENT / m_hostRangeSize : 0;

            result.status = QnManualCameraSearchStatus(m_state, currentProgress, MAX_PERCENT);

            NX_LOG(lit(" -----------------2 %1 : %2")
                .arg(result.status.current)
                .arg(result.status.total), 
                cl_logDEBUG1 );
            break;
        }
        case QnManualCameraSearchStatus::CheckingHost:
        {
            auto currentPercent = m_totalTaskCount ? 
               std::floor(0.5  
                    + (static_cast<double>(MAX_PERCENT) - PORT_SCAN_MAX_PROGRESS_PERCENT) 
                    * (m_totalTaskCount - m_remainingTaskCount) 
                    / m_totalTaskCount) + PORT_SCAN_MAX_PROGRESS_PERCENT : 0;

            result.status = QnManualCameraSearchStatus(
                m_state,
                currentPercent,
                MAX_PERCENT); 

            break;
        }
        default:
        {
            result.status = QnManualCameraSearchStatus(m_state, 0, MAX_PERCENT);
            break;
        }
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
        QnMutexLocker lock(&m_mutex);
        m_hostRangeSize = endIPv4Addr - startIPv4Addr;
        m_state = QnManualCameraSearchStatus::CheckingOnline;

        if (m_cancelled)
        {
            m_state = QnManualCameraSearchStatus::Aborted;
            return QStringList();
        }
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

void QnManualCameraSearcher::runTasksUnsafe(QThreadPool* threadPool)
{
    if (m_cancelled)
    {
        m_state = QnManualCameraSearchStatus::Aborted;
        return;
    }

    int totalRunningTasks = 0;

    for (const auto& context: m_searchQueueContexts)
    {
        if (!context.isInterrupted)
            totalRunningTasks += context.runningTaskCount;
    }

    auto canRunTask = 
        [&totalRunningTasks, threadPool, this]() -> bool
        {
            bool allQueuesAreBlocked = true;
            for (const auto& queueName: m_urlSearchTaskQueues.keys())
            {
                auto& context = m_searchQueueContexts[queueName];
                if (!context.isBlocked)
                {
                    allQueuesAreBlocked = false;
                    break;
                }
            }
            return (totalRunningTasks <= threadPool->maxThreadCount())
                && !m_urlSearchTaskQueues.isEmpty()
                && !allQueuesAreBlocked;
        };

    /**
     * We are launching tasks until count of running tasks do not exceeds 
     * maximum thread count in pool.
     */
    while(canRunTask())
    {
        // Take one task from each queue.
        for (auto it = m_urlSearchTaskQueues.begin(); it != m_urlSearchTaskQueues.end();)
        {
            auto queueName = it.key();

            auto& queue = m_urlSearchTaskQueues[queueName];
            auto& context = m_searchQueueContexts[queueName];

            if (it->isEmpty() || context.isInterrupted)
            {
                it = m_urlSearchTaskQueues.erase(it);
                continue;
            }
            else
            {
                it++;
            }

            if (!canRunTask())
                break;

            if (context.isBlocked)
                continue;

            QnSearchTask task = queue.dequeue();

            auto taskFn = std::bind(&QnSearchTask::doSearch, task);
            QnConcurrent::run(threadPool, taskFn);

            context.runningTaskCount++;
            totalRunningTasks++;

            if (task.isBlocking())
            {
                context.isBlocked = true;
                break;
            }
        }
    }
}
