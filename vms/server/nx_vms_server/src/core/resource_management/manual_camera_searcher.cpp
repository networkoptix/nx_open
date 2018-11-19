
#include "manual_camera_searcher.h"

#include <type_traits>
#include <functional>

#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrentMap>

#include <utils/common/scoped_thread_rollback.h>
#include <nx/utils/log/log.h>

#include <core/resource_management/camera_driver_restriction_list.h>
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


// TODO: Add UUID here.
QnManualCameraSearcher::QnManualCameraSearcher(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule),
    m_state(QnManualResourceSearchStatus::Init),
    m_cancelled(false),
    m_hostRangeSize(0),
    m_totalTaskCount(0),
    m_remainingTaskCount(0)
{
    NX_VERBOSE(this, "Created");
}

QnManualCameraSearcher::~QnManualCameraSearcher()
{
    NX_VERBOSE(this, "Destroying (state: %1)", m_state);
    cancel();
    // TODO: should make sure that all tasks has finished and notify cond var!
}

QList<QnAbstractNetworkResourceSearcher*> QnManualCameraSearcher::getAllNetworkSearchers() const
{
    QList<QnAbstractNetworkResourceSearcher*> result;

    for (QnAbstractResourceSearcher* as : commonModule()->resourceDiscoveryManager()->plugins())
    {
        QnAbstractNetworkResourceSearcher* ns =
            dynamic_cast<QnAbstractNetworkResourceSearcher*>(as);
        NX_ASSERT(ns);

        result.push_back(ns);
    }

    return result;
}

// TODO: Use QnIpRangeCheckerAsync asynchronously.
bool QnManualCameraSearcher::run(
    SearchDoneCallback callback,
    const QString& startAddr,
    const QString& endAddr,
    const QAuthenticator& auth,
    int port)
{
    using namespace std::placeholders;

    {
        QnMutexLocker lock( &m_mutex );
        // TODO: Use assert? Allow to run only from init state?
        if (m_state == QnManualResourceSearchStatus::Aborted)
            return false;

        m_totalTaskCount = 0;
        m_remainingTaskCount = 0;
        m_urlSearchTaskQueues.clear();
        m_searchQueueContexts.clear();
        m_cancelled = false;
        m_searchDoneCallback = std::move(callback);
    }

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
    NX_VERBOSE(this, "Will use %1 sequential and %2 concurrent searchers",
        sequentialSearchers.size(), parallelSearchers.size());

    QStringList onlineHosts;
    if (endAddr.isNull())
        onlineHosts.push_back(startAddr);
    else
        onlineHosts = getOnlineHosts(startAddr, endAddr, port);
    NX_VERBOSE(this, "Will check %1 hosts", onlineHosts.size());

    for (const auto& host: onlineHosts)
    {
        QnSearchTask sequentialTask(commonModule(), host, port, auth, true);
        sequentialTask.setSearchers(sequentialSearchers);
        sequentialTask.setSearchDoneCallback(
            std::bind(&QnManualCameraSearcher::searchTaskDoneHandler, this, _1, _2));
        sequentialTask.setBlocking(true);
        sequentialTask.setInterruptTaskProcessing(true);

        QString hostToCheck;
        hostToCheck = sequentialTask.url().toString();

        m_urlSearchTaskQueues[hostToCheck].enqueue(sequentialTask);
        m_searchQueueContexts[hostToCheck] = SearchTaskQueueContext();

        m_remainingTaskCount++;

        for (const auto& searcher: parallelSearchers)
        {
            QnSearchTask::SearcherList searcherList;
            searcherList.push_back(searcher);

            QnSearchTask parallelTask(commonModule(), host, port, auth, false);
            parallelTask.setSearchers(searcherList);
            sequentialTask.setSearchDoneCallback(
                std::bind(&QnManualCameraSearcher::searchTaskDoneHandler, this, _1, _2));

            m_urlSearchTaskQueues[hostToCheck].enqueue(parallelTask);

            m_remainingTaskCount++;
        }
    }

    m_totalTaskCount = m_remainingTaskCount;
    {
        QnMutexLocker lock(&m_mutex);
        if (m_cancelled)
            return true;
        changeStateUnsafe(QnManualResourceSearchStatus::CheckingHost);
        runTasksUnsafe();
    }

    // TODO: Should I run callback here?
    return true;
}

void QnManualCameraSearcher::cancel() {

    {
        QnMutexLocker lock(&m_mutex);
        NX_VERBOSE(this, "Canceling search, remaining tasks: %1", m_remainingTaskCount);
        m_cancelled = true;
        switch (m_state)
        {
            case QnManualResourceSearchStatus::Finished:
                return;
            case QnManualResourceSearchStatus::CheckingOnline:
                m_ipChecker.pleaseStop();
                break;
            default:
                break;
        }
        changeStateUnsafe(QnManualResourceSearchStatus::Aborted);
    }

    // TODO: Add state aborting.
    m_ipChecker.join();
//    waitToFinishSearch(); // TODO: should somehow wait here?
}

QnManualCameraSearchProcessStatus QnManualCameraSearcher::status() const
{
    QnMutexLocker lock(&m_mutex);
    QnManualCameraSearchProcessStatus result;

    switch (m_state)
    {
        case QnManualResourceSearchStatus::CheckingHost:
        {
            // TODO: Why saving result here? Can it have any cameras?
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
            NX_DEBUG(this, lit("%1 : %2")
                .arg(result.status.current)
                .arg(result.status.total));
            break;
        }
        case QnManualResourceSearchStatus::CheckingOnline:
        {
            Q_ASSERT(m_hostRangeSize);
            int currentProgress = m_hostRangeSize
                ? (int) m_ipChecker.hostsChecked() * PORT_SCAN_MAX_PROGRESS_PERCENT
                    / m_hostRangeSize
                : 0;

            result.status = QnManualResourceSearchStatus(m_state, currentProgress, MAX_PERCENT);

            NX_DEBUG(this, lit("%1 : %2")
                .arg(result.status.current)
                .arg(result.status.total));
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
    NX_VERBOSE(this, "Getting online hosts in range (%1, %2)", startAddr, endAddr);
    QStringList onlineHosts;

    const quint32 startIPv4Addr = QHostAddress(startAddr).toIPv4Address();
    const quint32 endIPv4Addr = QHostAddress(endAddr).toIPv4Address();

    if (endIPv4Addr < startIPv4Addr)
        return QStringList();

    {
        QnMutexLocker lock(&m_mutex);
        m_hostRangeSize = endIPv4Addr - startIPv4Addr;
        if (m_cancelled)
            return QStringList();
        changeStateUnsafe(QnManualResourceSearchStatus::CheckingOnline);
    }

    onlineHosts = m_ipChecker.onlineHosts(
        QHostAddress(startAddr),
        QHostAddress(endAddr),
        port ? port : nx::network::http::DEFAULT_HTTP_PORT );

    {
        QnMutexLocker lock( &m_mutex );
        if (m_cancelled)
            return QStringList();
    }

    return onlineHosts;
}

void QnManualCameraSearcher::searchTaskDoneHandler(
    const QnManualResourceSearchList &results, QnSearchTask * const task)
{
    QnMutexLocker lock(&m_mutex);
    auto queueName = task->url().toString();

    m_remainingTaskCount--;

    auto& context = m_searchQueueContexts[queueName];

    context.isBlocked = false;
    context.runningTaskCount--;

    if (!m_cancelled)
    {
        m_results.append(results);

        if (task->doesInterruptTaskProcessing() && !results.isEmpty())
        {
            m_remainingTaskCount -= m_urlSearchTaskQueues[queueName].size();
            context.isInterrupted = true;
        }

        runTasksUnsafe();
    }

    const auto hasRunningTasks =
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

    if ((m_remainingTaskCount > 0 && !m_cancelled) || hasRunningTasks())
        return;

    if (!m_cancelled)
        changeStateUnsafe(QnManualResourceSearchStatus::Finished);
    else
        NX_ASSERT(m_state == QnManualResourceSearchStatus::Aborted);
    NX_VERBOSE(this, "Search has finished, found %1 resources", m_results.size());

    NX_ASSERT(m_searchDoneCallback);
    m_searchDoneCallback(this);
}

void QnManualCameraSearcher::changeStateUnsafe(QnManualResourceSearchStatus::State newState)
{
    NX_VERBOSE(this, "State change: %1 -> %2", m_state, newState);
    m_state = newState;
}

void QnManualCameraSearcher::runTasksUnsafe()
{
    if (m_cancelled)
        return;

    QThreadPool* threadPool = commonModule()->resourceDiscoveryManager()->threadPool();

    int totalRunningTasks = 0;
    for (const auto& context: m_searchQueueContexts)
    {
        if (!context.isInterrupted)
            totalRunningTasks += context.runningTaskCount;
    }

    // TODO: make normal function
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

            it++;

            if (!canRunTask())
                break;

            if (context.isBlocked)
                continue;

            QnSearchTask task = queue.dequeue();

            auto taskFn = std::bind(&QnSearchTask::doSearch, task); // TODO: Is it copy of task?
            nx::utils::concurrent::run(threadPool, taskFn); //< TODO: Change to lambda.

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
