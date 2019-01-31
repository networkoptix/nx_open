#pragma once

#include <QtCore/QSize>

#include <core/resource/resource_fwd.h>

#include <nx/vms/server/server_module_aware.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace vms::server {
namespace recorder {

class AbstractRemoteArchiveSynchronizationTask:
    public nx::vms::server::ServerModuleAware
{
public:
    AbstractRemoteArchiveSynchronizationTask(QnMediaServerModule* serverModule):
        nx::vms::server::ServerModuleAware(serverModule)
    {
    }

    virtual ~AbstractRemoteArchiveSynchronizationTask() {};
    virtual QnUuid id() const = 0;
    virtual void setDoneHandler(nx::utils::MoveOnlyFunc<void()> handler) = 0; // TODO #wearable Unused! Drop!
    virtual void cancel() = 0;
    virtual bool execute() = 0;
};

using RemoteArchiveTaskPtr = std::shared_ptr<AbstractRemoteArchiveSynchronizationTask>;

} // namespace recorder
} // namespace vms::server
} // namespace nx
