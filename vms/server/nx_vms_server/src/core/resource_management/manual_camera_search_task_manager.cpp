#include "manual_camera_search_task_manager.h"

#include <QtCore/QThreadPool>

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

            // TODO: #dliman All running tasks should be canceled here but they do not support
            // cancelation.

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
    nx::utils::Url url,
    std::vector<QnAbstractNetworkResourceSearcher*> searchers,
    bool isSequential)
{
    using namespace std::placeholders;

    // Task runs searchers sequentially, so if we want search to be parallel we should not run more
    // than one searcher in a task.
    NX_ASSERT(isSequential == true || searchers.size() == 1);
    QnSearchTask task(commonModule(), std::move(url), /*breakOnGotResult*/ isSequential);

    task.setSearchers(searchers);
    task.setSearchDoneCallback(
        std::bind(&QnManualSearchTaskManager::onSearchTaskDone, this, _1, _2));

    if (isSequential)
    {
        task.setBlocking(true);
        task.setInterruptTaskProcessing(true);
    }

    m_pollable.dispatch(
        [this, task = std::move(task), isSequential]() mutable
        {
            const auto url = task.url();
            NX_VERBOSE(this, "Adding task %1 on %2 (sequential: %3)", task, url, isSequential);
            NX_CRITICAL(m_state == State::init); //< NOTE: It is not supported to add tasks after run.
            m_searchTasksQueues[url].push(std::move(task));
            m_searchQueueContexts[url]; //< Create if does not exist.

            m_totalTaskCount++;
        });
}


void QnManualSearchTaskManager::startTasks(TasksFinishedCallback callback)
{
    m_pollable.dispatch(
        [this, callback = std::move(callback)]() mutable
        {
            NX_VERBOSE(this, "Running %1 tasks", m_totalTaskCount.load());
            NX_CRITICAL(m_state == State::init); //< NOTE: It supports being started only once.
            m_state = State::running;
            m_remainingTaskCount = m_totalTaskCount.load();
            m_runningTaskCount = 0;
            m_tasksFinishedCallback = std::move(callback);

            if (m_remainingTaskCount == 0)
                return onSearchFinished();
            runSomePendingTasks();
        });
}

double QnManualSearchTaskManager::doneToTotalTasksRatio() const
{
    int totalTasks = m_totalTaskCount;
    if (totalTasks == 0)
        return 0;

    return (totalTasks - m_remainingTaskCount) / double(totalTasks);
}

QnManualResourceSearchList QnManualSearchTaskManager::foundResources() const
{
    return m_pollable.executeInAioThreadSync(
        [this]() -> QnManualResourceSearchList { return m_foundResources; });
}

void QnManualSearchTaskManager::onSearchTaskDone(
    QnManualResourceSearchList results, QnSearchTask* const task)
{
    NX_VERBOSE(this, "Task %1 done", *task);
    bool taskInterruptProcessing = task->doesInterruptTaskProcessing();
    auto taskUrl = task->url();

    // NOTE: Task may be deleted after call to dispatch.
    m_pollable.dispatch(
        [this, results = std::move(results), taskInterruptProcessing, taskUrl = std::move(taskUrl)]()
        {
            NX_VERBOSE(this, "Remained: %1; Running: %2; Url: %3",
                m_remainingTaskCount.load(), m_runningTaskCount, taskUrl);

            auto& context = m_searchQueueContexts[taskUrl];

            context.isBlocked = false;
            context.runningTaskCount--;
            m_remainingTaskCount--;
            m_runningTaskCount--;

            if (m_state == State::running)
            {
                m_foundResources.append(results);

                if (taskInterruptProcessing && !results.isEmpty())
                {
                    m_remainingTaskCount -= int(m_searchTasksQueues[taskUrl].size());
                    context.isInterrupted = true;
                }

                runSomePendingTasks();
            }

            // Should not do anything else if it was not the last task ran.
            if (m_runningTaskCount > 0)
                return;
            onSearchFinished();
    });
}

void QnManualSearchTaskManager::onSearchFinished()
{
    NX_CRITICAL(m_pollable.isInSelfAioThread());

    // It is an error if the state is running and there are some pending tasks, but no
    // tasks are running.
    NX_ASSERT(m_runningTaskCount == 0);
    NX_ASSERT((m_state == State::running && m_remainingTaskCount == 0)
        || m_state == State::canceled);

    m_state = State::finished;
    NX_VERBOSE(this, "Search has finished, found %1 resources", m_foundResources.size());

    m_tasksFinishedCallback(std::move(m_foundResources));
}

void QnManualSearchTaskManager::runSomePendingTasks()
{
    NX_CRITICAL(m_pollable.isInSelfAioThread());
    NX_CRITICAL(m_state == State::running);

    QThreadPool* threadPool = commonModule()->resourceDiscoveryManager()->threadPool();

    while (canRunTask(threadPool))
    {
        // Take one task from each queue.
        for (auto it = m_searchTasksQueues.begin(); it != m_searchTasksQueues.end();)
        {
            const auto queueName = it->first;
            auto& queue = it->second;
            auto& context = m_searchQueueContexts[queueName];

            if (queue.empty() || context.isInterrupted)
            {
                it = m_searchTasksQueues.erase(it);
                continue;
            }
            it++;

            if (!canRunTask(threadPool))
                break;

            if (context.isBlocked)
                continue;

            bool isTaskBlocking = queue.front().isBlocking();
            nx::utils::concurrent::run(threadPool,
                std::bind(&QnSearchTask::start, std::move(queue.front())));
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
    NX_CRITICAL(m_pollable.isInSelfAioThread());
    bool allQueuesAreBlocked = true;
    for (const auto& [queueName, queue]: m_searchTasksQueues)
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
        && !m_searchTasksQueues.empty()
            && !allQueuesAreBlocked;
}
