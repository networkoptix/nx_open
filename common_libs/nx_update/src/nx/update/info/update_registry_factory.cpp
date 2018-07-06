#include <nx/utils/std/cpp14.h>
#include "update_registry_factory.h"
#include "impl/common_update_registry.h"

namespace nx {
namespace update {
namespace info {

UpdateRegistryFactoryFunction UpdateRegistryFactory::m_factoryFunction = nullptr;
EmptyUpdateRegistryFactoryFunction UpdateRegistryFactory::m_emptyFactoryFunction = nullptr;

namespace {
AbstractUpdateRegistryPtr createCommonUpdateRegistry(
    const QString& baseUrl,
    detail::data_parser::UpdatesMetaData metaData,
    detail::CustomizationVersionToUpdate customizationVersionToUpdate)
{
    return std::make_unique<impl::CommonUpdateRegistry>(
        baseUrl,
        std::move(metaData),
        std::move(customizationVersionToUpdate));
}

AbstractUpdateRegistryPtr createEmptyCommonUpdateRegistry()
{
    return std::make_unique<impl::CommonUpdateRegistry>();
}
} // namespace

AbstractUpdateRegistryPtr UpdateRegistryFactory::create(
    const QString& baseUrl,
    detail::data_parser::UpdatesMetaData metaData,
    detail::CustomizationVersionToUpdate customizationVersionToUpdate)
{
    if (m_factoryFunction)
    {
        return m_factoryFunction(
            baseUrl,
            std::move(metaData),
            std::move(customizationVersionToUpdate));
    }

    return createCommonUpdateRegistry(
        baseUrl,
        std::move(metaData),
        std::move(customizationVersionToUpdate));
}

AbstractUpdateRegistryPtr UpdateRegistryFactory::create()
{
    if (m_emptyFactoryFunction)
        return m_emptyFactoryFunction();

    return createEmptyCommonUpdateRegistry();
}

void UpdateRegistryFactory::setFactoryFunction(UpdateRegistryFactoryFunction factoryFunction)
{
    m_factoryFunction = std::move(factoryFunction);
}

void UpdateRegistryFactory::setEmptyFactoryFunction(
    EmptyUpdateRegistryFactoryFunction emptyFactoryFunction)
{
    m_emptyFactoryFunction = std::move(emptyFactoryFunction);
}

} // namespace info
} // namespace update
} // namespace nx
