#include "update_registry_factory.h"
#include "common_update_registry.h"

namespace nx {
namespace update {
namespace info {
namespace impl {

UpdateRegistryFactoryFunction UpdateRegistryFactory::m_factoryFunction = nullptr;

namespace {
AbstractUpdateRegistryPtr createCommonUpdateRegistry()
{
    return std::make_unique<CommonUpdateRegistry>();
}

} // namespace

AbstractUpdateRegistryPtr UpdateRegistryFactory::create()
{
    if (m_factoryFunction)
        return m_factoryFunction();

    return createCommonUpdateRegistry();
}

void UpdateRegistryFactory::setFactoryFunction(UpdateRegistryFactoryFunction factoryFunction)
{
    m_factoryFunction = factoryFunction;
}

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
