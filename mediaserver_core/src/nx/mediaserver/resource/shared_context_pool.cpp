#include "shared_context_pool.h"

namespace nx {
namespace mediaserver {
namespace resource {

SharedContextPool::SharedContextPool(QnMediaServerModule* serverModule, QObject* parent):
    QObject(parent),
    ServerModuleAware(serverModule)
{
}

} // namespace resource
} // namespace mediaserver
} // namespace nx
