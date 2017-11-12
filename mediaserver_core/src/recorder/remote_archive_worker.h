#pragma once

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/uuid.h>
#include <recorder/abstract_remote_archive_synchronization_task.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

using RemoteArchiveTaskDoneHandler = std::function<void(const QnUuid&)>;

class RemoteArchiveWorker: public QnLongRunnable
{
    using base_class = QnLongRunnable;

public:
    RemoteArchiveWorker() = default;
    virtual ~RemoteArchiveWorker();

    virtual void run() override;

    virtual void pleaseStop() override;

    void setTask(const RemoteArchiveTaskPtr& task);

    void cancel();

    QnUuid taskId() const;

    void setTaskDoneHandler(RemoteArchiveTaskDoneHandler taskDoneHandler);

private:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_wait;
    RemoteArchiveTaskPtr m_task;
    RemoteArchiveTaskDoneHandler m_taskDoneHandler;
    std::atomic<bool> m_terminated{false};
};


} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
