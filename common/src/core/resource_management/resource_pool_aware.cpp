#include "resource_pool_aware.h"

#include <common/common_module.h>
#include <common/common_module_aware.h>
#include <core/resource_management/resource_pool.h>

namespace nx {
namespace core {
namespace resource {

ResourcePoolAware::ResourcePoolAware(QnCommonModule* common):
    QnCommonModuleAware(common)
{
    auto resourcePool = commonModule()->resourcePool();
    NX_ASSERT(resourcePool, "There is no resource pool!");

    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        this, &ResourcePoolAware::resourceAdded);

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        this, &ResourcePoolAware::resourceRemoved);

}

ResourcePoolAware::~ResourcePoolAware()
{

}

} // namespace resource
} // namespace core
} // namespace nx