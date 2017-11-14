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
    RemoteArchiveWorker(RemoteArchiveTaskPtr task);

    virtual void run() override;

    virtual void pleaseStop() override;

    void setTaskDoneHandler(RemoteArchiveTaskDoneHandler taskDoneHandler);

private:
    RemoteArchiveTaskPtr m_task;
    RemoteArchiveTaskDoneHandler m_taskDoneHandler;
};


} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
