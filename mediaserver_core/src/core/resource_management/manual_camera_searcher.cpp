
#include "manual_camera_searcher.h"

#include <type_traits>

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentMap>

#include <utils/common/scoped_thread_rollback.h>
#include <nx/utils/log/log.h>
#include <nx/network/ip_range_checker.h>

#include <core/resource_management/camera_driver_restriction_list.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_searcher.h>

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <plugins/resource/archive_camera/archive_camera.h>
#include <common/common_module.h>


static const int MAX_PERCENT = 100;
//assuming that scanning ports is ten times faster than plugin check
static const int PORT_SCAN_MAX_PROGRESS_PERCENT = 10;
static_assert( PORT_SCAN_MAX_PROGRESS_PERCENT < MAX_PERCENT, "PORT_SCAN_MAX_PROGRESS_PERCENT must be less than MAX_PERCENT" );

namespace {

    bool restrictNewManualCameraByIP(const QnSecurityCamResourcePtr& netRes)
    {
        if (netRes->hasCameraCapabilities(Qn::ShareIpCapability))
            return false; //< don't block

        auto resCommonModule = netRes->commonModule();
        NX_ASSERT(resCommonModule, lit("Common module should be set for resource"));
        if (!resCommonModule)
            return true; //< Don't add resource without properly set common module

        auto resPool = resCommonModule->resourcePool();
        NX_ASSERT(resPool, "Resource should have correspondent resource pool");
        if (!resPool)
            return true; // Don't add resource without properly set resource pool

        QnNetworkResourceList existResList = resPool->getAllNetResourceByHostAddress(netRes->getHostAddress());
        existResList = existResList.filtered(
            [&netRes](const QnNetworkResourcePtr& existRes)
            {
                bool sameParent = netRes->getParentId() == existRes->getParentId();
                return sameParent && existRes->getStatus() != Qn::Offline;
            });

        for (const QnNetworkResourcePtr& existRes: existResList)
        {
            QnSecurityCamResourcePtr existCam = existRes.dynamicCast<QnSecurityCamResource>();
            if (!existCam)
                continue;

            if (existCam->hasCameraCapabilities(Qn::ShareIpCapability))
                return false; //< don't block

            if (!existCam->isManuallyAdded())
                return true; //< block manual and auto camera at same IP

            if (existRes->getTypeId() != netRes->getTypeId())
            {
                // allow several manual cameras with the same IP if cameras have different ports
                QUrl url1(existRes->getUrl());
                QUrl url2(netRes->getUrl());
                if (url1.port() == url2.port())
                    return true; //< camera found by different drivers with the same port
            }
        }
        return false;
    }

    QnManualResourceSearchEntry entryFromCamera(const QnSecurityCamResourcePtr &camera)
    {
        return QnManualResourceSearchEntry(
              camera->getName()
            , camera->getUrl()
            , qnResTypePool->getResourceType(camera->getTypeId())->getName()
            , camera->getVendor()
            , camera->getUniqueId()
            , QnResourceDiscoveryManager::findSameResource(camera)
              || restrictNewManualCameraByIP(camera)
            );
    }

    QnManualResourceSearchEntry entryFromResource(const QnResourcePtr &resource)
    {
        if (const QnSecurityCamResourcePtr &camera = resource.dynamicCast<QnSecurityCamResource>())
            return entryFromCamera(camera);

        return QnManualResourceSearchEntry();
    }
}

QnSearchTask::QnSearchTask(
    const QString& addr,
    int port,
    const QAuthenticator& auth,
    bool breakOnGotResult)
:
    m_auth(auth),
    m_breakIfGotResult(breakOnGotResult),
    m_blocking(false),
    m_interruptTaskProcesing(false)
{
    if (QUrl(addr).scheme().isEmpty())
        m_url.setHost(addr);
    else
        m_url.setUrl(addr);

    if (!m_url.userInfo().isEmpty())
    {
        m_auth.setUser(m_url.userName());
        m_auth.setPassword(m_url.password());
        m_url.setUserInfo(QString());
    }

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
    QnManualResourceSearchList results;
    for (const auto& checker: m_searchers)
    {
        auto seqResults = checker->checkHostAddr(m_url, m_auth, true);

        for (const auto& res: seqResults)
        {
            res->setCommonModule(checker->commonModule());
            QnSecurityCamResourcePtr camRes = res.dynamicCast<QnSecurityCamResource>();
            Q_ASSERT(camRes);
            // Checking, if found resource is reserved by some other searcher
            if(camRes && !CameraDriverRestrictionList::instance()->driverAllowedForCamera(
                checker->manufacture(),
                camRes->getVendor(),
                camRes->getModel()))
            {
                continue;   //< Camera is not allowed to be used with this driver.
            }

            results.push_back(entryFromResource(res));
        }

        if (!results.isEmpty() && m_breakIfGotResult)
        {
            m_callback(results, this);
            return;
        }
    }

    m_callback(results, this);
}

nx::utils::Url QnSearchTask::url()
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

QnManualCameraSearcher::QnManualCameraSearcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_state(QnManualResourceSearchStatus::Init),
    m_cancelled(false),
    m_hostRangeSize(0),
    m_totalTaskCount(0),
    m_remainingTaskCount(0)
{
}

QnManualCameraSearcher::~QnManualCameraSearcher()
{
}

QList<QnAbstractNetworkResourceSearcher*> QnManualCameraSearcher::getAllNetworkSearchers() const
{
    QList<QnAbstractNetworkResourceSearcher*> result;

    for (QnAbstractResourceSearcher* as : commonModule()->resourceDiscoveryManager()->plugins())
    {
        QnAbstractNetworkResourceSearcher* ns =
            dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
        Q_ASSERT(ns);

        result.push_back(ns);
    }

    return result;
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
        if (m_state == QnManualResourceSearchStatus::Aborted)
            return false;

        m_totalTaskCount = 0;
        m_remainingTaskCount = 0;
        m_urlSearchTaskQueues.clear();
        m_searchQueueContexts.clear();
        m_cancelled = false;
    }

    QnSearchTask::SearchDoneCallback callback =
        [this, threadPool](const QnManualResourceSearchList& results, QnSearchTask* const task)
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
    m_state = QnManualResourceSearchStatus::CheckingHost;
    if (m_cancelled)
    {
        m_state = QnManualResourceSearchStatus::Aborted;
        return true;
    }

    runTasksUnsafe(threadPool);

    while((m_remainingTaskCount > 0 && !m_cancelled) || hasRunningTasks())
        m_waitCondition.wait(&m_mutex, kMaxWaitTimeMs);

    if (!m_cancelled)
        m_state = QnManualResourceSearchStatus::Finished;

    return true;
}

void QnManualCameraSearcher::cancel() {

    QnMutexLocker lock( &m_mutex );
    m_cancelled = true;

    switch (m_state)
    {
        case QnManualResourceSearchStatus::Finished:
        {
            return;
        }
        case QnManualResourceSearchStatus::CheckingOnline:
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

    m_state = QnManualResourceSearchStatus::Aborted;
}

QnManualCameraSearchProcessStatus QnManualCameraSearcher::status() const
{
    QnMutexLocker lock( &m_mutex );
    QnManualCameraSearchProcessStatus result;

    switch (m_state)
    {
        case QnManualResourceSearchStatus::CheckingHost:
        {
            result.cameras = m_results;
            break;
        }
        case QnManualResourceSearchStatus::Finished:
        case QnManualResourceSearchStatus::Aborted:
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
        case QnManualResourceSearchStatus::Finished:
        case QnManualResourceSearchStatus::Aborted:
        {
            result.status = QnManualResourceSearchStatus(m_state, MAX_PERCENT, MAX_PERCENT);
            NX_LOG(lit(" -----------------1 %1 : %2")
                .arg(result.status.current)
                .arg(result.status.total),
                cl_logDEBUG1 );
            break;
        }
        case QnManualResourceSearchStatus::CheckingOnline:
        {
            Q_ASSERT(m_hostRangeSize);
            int currentProgress = m_hostRangeSize ?
                m_ipChecker.hostsChecked() * PORT_SCAN_MAX_PROGRESS_PERCENT / m_hostRangeSize : 0;

            result.status = QnManualResourceSearchStatus(m_state, currentProgress, MAX_PERCENT);

            NX_LOG(lit(" -----------------2 %1 : %2")
                .arg(result.status.current)
                .arg(result.status.total),
                cl_logDEBUG1 );
            break;
        }
        case QnManualResourceSearchStatus::CheckingHost:
        {
            auto currentPercent = m_totalTaskCount ?
               std::floor(0.5
                    + (static_cast<double>(MAX_PERCENT) - PORT_SCAN_MAX_PROGRESS_PERCENT)
                    * (m_totalTaskCount - m_remainingTaskCount)
                    / m_totalTaskCount) + PORT_SCAN_MAX_PROGRESS_PERCENT : 0;

            result.status = QnManualResourceSearchStatus(
                m_state,
                currentPercent,
                MAX_PERCENT);

            break;
        }
        default:
        {
            result.status = QnManualResourceSearchStatus(m_state, 0, MAX_PERCENT);
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
        m_state = QnManualResourceSearchStatus::CheckingOnline;

        if (m_cancelled)
        {
            m_state = QnManualResourceSearchStatus::Aborted;
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
            m_state = QnManualResourceSearchStatus::Aborted;
            return QStringList();
        }
    }

    return onlineHosts;
}

void QnManualCameraSearcher::runTasksUnsafe(QThreadPool* threadPool)
{
    if (m_cancelled)
    {
        m_state = QnManualResourceSearchStatus::Aborted;
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
            nx::utils::concurrent::run(threadPool, taskFn);

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
