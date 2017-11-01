#pragma once

#include <QtCore/QSize>

#include <core/resource/resource_fwd.h>
#include <nx/mediaserver/server_module_aware.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

static const QSize kMaxLowStreamResolution(1024, 768);
static const QString kArchiveContainer("matroska");
static const QString kArchiveContainerExtension(".mkv");

class AbstractRemoteArchiveSynchronizationTask:
    public nx::mediaserver::ServerModuleAware
{
public:
    AbstractRemoteArchiveSynchronizationTask(QnMediaServerModule* serverModule):
        nx::mediaserver::ServerModuleAware(serverModule)
    {
    }

    virtual ~AbstractRemoteArchiveSynchronizationTask() {};
    virtual void setDoneHandler(std::function<void()> handler) = 0;
    virtual void cancel() = 0;
    virtual bool execute() = 0;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx
