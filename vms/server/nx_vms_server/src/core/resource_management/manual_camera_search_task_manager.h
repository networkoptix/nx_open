#pragma once

#include <map>
#include <queue>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/async_stoppable.h>
#include <nx/utils/move_only_func.h>
#include <nx/network/socket_common.h>

#include "manual_camera_search_task.h"

class QnManualSearchTaskManager: public /*mixin*/ QnCommonModuleAware, public nx::network::QnStoppableAsync
{
public:
    using TasksFinishedCallback = nx::utils::MoveOnlyFunc<void(QnManualResourceSearchList)>;

    struct SearchTaskQueueContext
    {
        bool isBlocked = false;
        bool isInterrupted = false;
        int runningTaskCount = 0;
    };

    QnManualSearchTaskManager(
        QnCommonModule* commonModule, nx::network::aio::AbstractAioThread* thread);
    virtual ~QnManualSearchTaskManager() override;

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler) override;

    void addTask(nx::utils::Url url,
        std::vector<QnAbstractNetworkResourceSearcher *> searchers,
        bool isSequential); //< Otherwise it is parallel.

    void startTasks(TasksFinishedCallback callback);

    double doneToTotalTasksRatio() const;
    QnManualResourceSearchList foundResources() const;

private:
    enum class State { init, running, finished, canceled };

    bool canRunTask(QThreadPool* threadPool);
    void runSomePendingTasks();

    void onSearchTaskDone(QnManualResourceSearchList results, QnSearchTask* const task);
    void onSearchFinished();

    QnManualResourceSearchList m_foundResources;
    TasksFinishedCallback m_tasksFinishedCallback;

    std::atomic<int> m_totalTaskCount = 0;
    std::atomic<int> m_remainingTaskCount = 0;
    int m_runningTaskCount = 0;
    std::atomic<State> m_state = State::init;

    mutable nx::network::aio::BasicPollable m_pollable;

    std::map<nx::utils::Url, std::queue<QnSearchTask>> m_searchTasksQueues;
    std::map<nx::utils::Url, SearchTaskQueueContext> m_searchQueueContexts;
};
