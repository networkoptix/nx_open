#include "manual_camera_search_task_manager.h"

#include <nx/utils/unused.h>
#include <nx/utils/log/log.h>
#include <nx/utils/concurrent.h>
#include <common/common_module.h>
#include <core/resource_management/resource_discovery_manager.h>

QnManualSearchTaskManager::QnManualSearchTaskManager(
    QnCommonModule *commonModule, nx::network::aio::AbstractAioThread *thread)
    :
    QnCommonModuleAware(commonModule),
    m_pollable(thread)
{
}

QnManualSearchTaskManager::~QnManualSearchTaskManager()
{
    pleaseStopSync();
}

void QnManualSearchTaskManager::pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    m_pollable.dispatch(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            NX_VERBOSE(this, "PleaseStop called, running tasks: %1", m_runningTaskCount);
            m_state = State::canceled;

            // TODO: #dliman All running tasks should be canceled here.

            // If there are no running tasks -- we should run handler right away, otherwise it will
            // be never called.
            if (m_runningTaskCount == 0)
            {
                m_pollable.pleaseStopSync();
                completionHandler();
            }
            else
            {
                // Replace original callback to the pleaseStop one.
                m_tasksFinishedCallback =
                    [this, handler = std::move(completionHandler)](QnManualResourceSearchList)
                    {
                        m_pollable.pleaseStopSync();
                        handler();
                    };
            }
        });
}

void QnManualSearchTaskManager::addTask(
    nx::network::SocketAddress address,
    const QAuthenticator& auth,
    std::vector<QnAbstractNetworkResourceSearcher*> searchers, bool isSequential)
{
    using namespace std::placeholders;

    QnSearchTask task(commonModule(), address, auth, /*breakOnGotResult*/ isSequential);

    task.setSearchers(searchers);
    task.setSearchDoneCallback(
        std::bind(&QnManualSearchTaskManager::searchTaskDoneHandler, this, _1, _2));

    if (isSequential)
    {
        task.setBlocking(true);
        task.setInterruptTaskProcessing(true);
    }

    m_pollable.dispatch(
        [this, task = std::move(task), address, isSequential]() mutable
        {
            NX_VERBOSE(this, "Adding task on %1 (sequential: %2)", task.url(), isSequential);
            NX_ASSERT(m_state == State::init); //< NOTE: It is not supported to add tasks after run.
            m_urlSearchTaskQueues[address.address].push(std::move(task));
            m_searchQueueContexts[address.address]; //< Create if does not exist.

            m_totalTaskCount++;
        });
}

void QnManualSearchTaskManager::startTasks(TasksFinishedCallback callback)
{
    m_pollable.dispatch(
        [this, callback = std::move(callback)]() mutable
        {
            NX_VERBOSE(this, "Running %1 tasks", m_totalTaskCount.load());
            NX_ASSERT(m_state == State::init); //< NOTE: It supports being started only once.
            m_state = State::running;
            m_remainingTaskCount = m_totalTaskCount.load();
            m_runningTaskCount = 0;
            m_tasksFinishedCallback = std::move(callback);

            runSomePendingTasks();
        });
}

double QnManualSearchTaskManager::doneToTotalTasksRatio() const
{
    int totalTasks = m_totalTaskCount;
    if (totalTasks == 0)
        return 0;

    return (totalTasks - m_remainingTaskCount) / totalTasks;
}

QnManualResourceSearchList QnManualSearchTaskManager::foundResources() const
{
    if (m_pollable.isInSelfAioThread())
        return m_foundResources;

    QnManualResourceSearchList result;
    std::promise<void> result_promise;
    auto result_ready = result_promise.get_future();
    m_pollable.post(
        [this, &result, &result_promise]()
        {
            result = m_foundResources;
            result_promise.set_value();
        });

    result_ready.wait();
    return result;
}

void QnManualSearchTaskManager::searchTaskDoneHandler(
    const QnManualResourceSearchList& results, QnSearchTask* const task)
{
    m_pollable.dispatch(
        [this, &results, task]()
        {
            NX_VERBOSE(this, "Remained: %1; Running: %2",
                m_remainingTaskCount.load(), m_runningTaskCount);

            nx::network::HostAddress queueName(task->url().host());
            auto& context = m_searchQueueContexts[queueName];

            context.isBlocked = false;
            context.runningTaskCount--;
            m_remainingTaskCount--;
            m_runningTaskCount--;

            if (m_state == State::running)
            {
                m_foundResources.append(results);

                if (task->doesInterruptTaskProcessing() && !results.isEmpty())
                {
                    m_remainingTaskCount -= m_urlSearchTaskQueues[queueName].size();
                    context.isInterrupted = true;
                }

                runSomePendingTasks();
            }

            // Should not do anything else if it was not the last task ran.
            if (m_runningTaskCount > 0)
                return;

            // It is an error if the state is running and there are some pending tasks, but no
            // tasks are running.
            NX_ASSERT(m_runningTaskCount == 0);
            NX_ASSERT((m_state == State::running && m_remainingTaskCount == 0)
                || m_state == State::canceled);

            m_state = State::finished;
            NX_VERBOSE(this, "Search has finished, found %1 resources", m_foundResources.size());

            m_tasksFinishedCallback(std::move(m_foundResources));
        });
}

void QnManualSearchTaskManager::runSomePendingTasks()
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
    NX_ASSERT(m_state == State::running);

    // TODO: #dliman Should it use its own thread pool?
    QThreadPool* threadPool = commonModule()->resourceDiscoveryManager()->threadPool();

    while (canRunTask(threadPool))
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

            if (!canRunTask(threadPool))
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

bool QnManualSearchTaskManager::canRunTask(QThreadPool *threadPool)
{
    NX_ASSERT(m_pollable.isInSelfAioThread());
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
