#include "shared_context_pool.h"

namespace nx {
namespace vms::server {
namespace resource {

SharedContextPool::SharedContextPool(QnMediaServerModule* serverModule, QObject* parent):
    QObject(parent),
    ServerModuleAware(serverModule)
{
}

} // namespace resource
} // namespace vms::server
} // namespace nx
