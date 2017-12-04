#include "update_registry_factory.h"
#include "common_update_registry.h"

namespace nx {
namespace update {
namespace info {
namespace impl {

UpdateRegistryFactoryFunction UpdateRegistryFactory::m_factoryFunction = nullptr;

namespace {
AbstractUpdateRegistryPtr createCommonUpdateRegistry(
    detail::data_parser::UpdatesMetaData metaData,
    detail::CustomizationVersionToUpdate customizationVersionToUpdate)
{
    return std::make_unique<CommonUpdateRegistry>(
        std::move(metaData),
        std::move(customizationVersionToUpdate));
}

} // namespace

AbstractUpdateRegistryPtr UpdateRegistryFactory::create(
    detail::data_parser::UpdatesMetaData metaData,
    detail::CustomizationVersionToUpdate customizationVersionToUpdate)
{
    if (m_factoryFunction)
        return m_factoryFunction();

    return createCommonUpdateRegistry(
        std::move(metaData),
        std::move(customizationVersionToUpdate));
}

void UpdateRegistryFactory::setFactoryFunction(UpdateRegistryFactoryFunction factoryFunction)
{
    m_factoryFunction = std::move(factoryFunction);
}

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
