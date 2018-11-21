#pragma once

#include <map>
#include <queue>

#include <nx/network/async_stoppable.h>
#include <nx/utils/move_only_func.h>
#include <nx/network/socket_common.h>
#include <nx/utils/thread/mutex.h>

#include "manual_camera_search_task.h"

// TODO: inherit QnStoppable? Any other useful abstract classes?
class QnManualSearchTaskManager: public QnCommonModuleAware, public nx::network::QnStoppableAsync
{
public:
    using TasksFinishedCallback = nx::utils::MoveOnlyFunc<void(QnManualResourceSearchList)>;

    struct SearchTaskQueueContext
    {
        bool isBlocked = false;
        bool isInterrupted = false;
        int runningTaskCount = 0;
    };

    QnManualSearchTaskManager(QnCommonModule* commonModule);
    ~QnManualSearchTaskManager();
    // TODO: it must wait in destr to end all tasks

    // TODO: Assert that if isSequential false -- searchers.size() <= 1. <- Why?
    // TODO: Rafactor sequential and parallel logic.
    void addTask(nx::network::SocketAddress address,
        const QAuthenticator& auth,
        const std::vector<QnAbstractNetworkResourceSearcher *> &searchers,
        bool isSequential); //< Otherwise it is parallel.

    void startTasks(TasksFinishedCallback callback);

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler);

    double doneToTotalTasksRatio() const;

private:
    enum class State { init, running, finished, canceled };

    bool canRunTaskUnsafe(QThreadPool* threadPool);
    void runSomePendingTasksUnsafe();

    void searchTaskDoneHandler(const QnManualResourceSearchList& results, QnSearchTask* const task);

    QnManualResourceSearchList m_foundResources;
    TasksFinishedCallback m_tasksFinishedCallback;

    // TODO: Check if need all.
    int m_totalTaskCount = 0;
    int m_remainingTaskCount = 0;
    int m_runningTaskCount = 0;
    std::atomic<State> m_state = State::init;

    mutable nx::utils::Mutex m_lock;  // TODO: use something newer?

    // TODO: Merge or refactor maps. Rename them
    std::map<nx::network::HostAddress, std::queue<QnSearchTask>> m_urlSearchTaskQueues;
    std::map<nx::network::HostAddress, SearchTaskQueueContext> m_searchQueueContexts;
};
