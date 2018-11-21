#include "manual_camera_search_task_manager.h"

#include <nx/utils/unused.h>
#include <nx/utils/log/log.h>
#include <nx/utils/concurrent.h>
#include <common/common_module.h>
#include <core/resource_management/resource_discovery_manager.h>

//#include <nx/utils/

QnManualSearchTaskManager::QnManualSearchTaskManager(QnCommonModule *commonModule):
    QnCommonModuleAware(commonModule)
{

}

QnManualSearchTaskManager::~QnManualSearchTaskManager()
{
    pleaseStopSync();
}

void QnManualSearchTaskManager::addTask(
    nx::network::SocketAddress address,
    const QAuthenticator& auth,
    const std::vector<QnAbstractNetworkResourceSearcher*>& searchers, bool isSequential)
{
    using namespace std::placeholders;
    // TODO: Add log message.

    QnSearchTask task(commonModule(), address, auth, /*breakOnGotResult*/ isSequential);

    task.setSearchers(searchers);
    task.setSearchDoneCallback(
        std::bind(&QnManualSearchTaskManager::searchTaskDoneHandler, this, _1, _2));

    if (isSequential)
    {
        task.setBlocking(true);
        task.setInterruptTaskProcessing(true);
    }

    NX_MUTEX_LOCKER lock(&m_lock);
    NX_ASSERT(m_state == State::init); //< NOTE: It is not supported to add tasks after run.
    m_urlSearchTaskQueues[address.address].push(std::move(task));
    m_searchQueueContexts[address.address]; //< Create if does not exist.

    m_totalTaskCount++;
}

void QnManualSearchTaskManager::startTasks(TasksFinishedCallback callback)
{
    NX_VERBOSE(this, "Running %1 tasks", m_totalTaskCount);

    NX_MUTEX_LOCKER lock(&m_lock);
    NX_ASSERT(m_state == State::init); //< NOTE: It supports being started only once.
    m_state = State::running;
    m_remainingTaskCount = m_totalTaskCount;
    m_runningTaskCount = 0;
    m_tasksFinishedCallback = std::move(callback);

    runSomePendingTasksUnsafe();
}

void QnManualSearchTaskManager::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    NX_MUTEX_LOCKER lock(&m_lock);
    NX_VERBOSE(this, "PleaseStop called, running tasks: %1", m_runningTaskCount);
    m_state = State::canceled;

    // TODO: is it OK that original handler for startTasks will not be called?

    // If there are no running tasks -- we should run handler right away, otherwise it will be
    // never called.
    if (m_runningTaskCount == 0)
    {
        nx::utils::Unlocker unlocker(&lock);
        completionHandler();
    }
    else
    {
        m_tasksFinishedCallback =
            [handler = std::move(completionHandler)](QnManualResourceSearchList) { handler(); };
    }
}

double QnManualSearchTaskManager::doneToTotalTasksRatio() const
{
    NX_MUTEX_LOCKER lock(&m_lock);
    if (m_totalTaskCount == 0)
        return 0;

    return (m_totalTaskCount - m_remainingTaskCount) / m_totalTaskCount;
}

void QnManualSearchTaskManager::searchTaskDoneHandler(
    const QnManualResourceSearchList& results, QnSearchTask* const task)
{
    {
        NX_MUTEX_LOCKER lock(&m_lock);
        nx::network::HostAddress queueName(task->url().host());
        auto& context = m_searchQueueContexts[queueName];

        context.isBlocked = false;
        context.runningTaskCount--;
        m_remainingTaskCount--;
        m_runningTaskCount--;

        NX_VERBOSE(this, "Remained: %1; Running: %2", m_remainingTaskCount, m_runningTaskCount);
        if (m_state == State::running)
        {
            m_foundResources.append(results);

            if (task->doesInterruptTaskProcessing() && !results.isEmpty())
            {
                m_remainingTaskCount -= m_urlSearchTaskQueues[queueName].size();
                context.isInterrupted = true;
            }

            runSomePendingTasksUnsafe();
        }

        // Should not do anything else if it was not the last task ran.
        if (m_runningTaskCount > 0)
            return;

        // It is an error if the state is running and there are some pending tasks, but no tasks are
        // running.
        NX_ASSERT(m_runningTaskCount == 0);
        NX_ASSERT((m_state == State::running && m_remainingTaskCount == 0) || m_state == State::canceled);

        m_state = State::finished;
        NX_VERBOSE(this, "Search has finished, found %1 resources", m_foundResources.size());
    }

    // TODO: Try to fix race: when called pleaseStop() right after unlock of mutex...
    // NOTE: Race is possible here, but it should not occur in real world.
    NX_ASSERT(m_tasksFinishedCallback);
    m_tasksFinishedCallback(std::move(m_foundResources));
}

void QnManualSearchTaskManager::runSomePendingTasksUnsafe()
{
    NX_ASSERT(m_state == State::running);

    // TODO: Should it use its own thread pool? Is it possible to use aio instead?
    QThreadPool* threadPool = commonModule()->resourceDiscoveryManager()->threadPool();

    while (canRunTaskUnsafe(threadPool))
    {
        // Take one task from each queue.
        for (auto it = m_urlSearchTaskQueues.begin(); it != m_urlSearchTaskQueues.end();)
        {
            auto queueName = it->first;
            auto& queue = it->second;
            auto& context = m_searchQueueContexts[queueName];

            if (queue.empty() || context.isInterrupted)
            {
                it = m_urlSearchTaskQueues.erase(it);
                continue;
            }
            it++;

            if (!canRunTaskUnsafe(threadPool))
                break;

            if (context.isBlocked)
                continue;

            bool isTaskBlocking = queue.front().isBlocking();
            nx::utils::concurrent::run(threadPool,
                std::bind(&QnSearchTask::doSearch, std::move(queue.front())));
            queue.pop();

            context.runningTaskCount++;
            m_runningTaskCount++;

            if (isTaskBlocking)
            {
                context.isBlocked = true;
                break; // TODO: Why break, not continue?
            }
        }

    }
}

bool QnManualSearchTaskManager::canRunTaskUnsafe(QThreadPool *threadPool)
{
    bool allQueuesAreBlocked = true;
    for (const auto& [queueName, queue]: m_urlSearchTaskQueues)
    {
        nx::utils::unused(queue);
        auto& context = m_searchQueueContexts[queueName];
        if (!context.isBlocked)
        {
            allQueuesAreBlocked = false;
            break;
        }
    }

    return (m_runningTaskCount <= threadPool->maxThreadCount())
        && !m_urlSearchTaskQueues.empty()
            && !allQueuesAreBlocked;
}
