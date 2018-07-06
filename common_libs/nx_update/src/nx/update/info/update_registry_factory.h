#pragma once

#include <memory>
#include <nx/update/info/abstract_update_registry.h>
#include <nx/utils/move_only_func.h>
#include <nx/update/info/detail/data_parser/updates_meta_data.h>
#include <nx/update/info/detail/customization_version_data.h>

namespace nx {
namespace update {
namespace info {

using AbstractUpdateRegistryPtr = std::unique_ptr<AbstractUpdateRegistry>;
using UpdateRegistryFactoryFunction =
    nx::utils::MoveOnlyFunc<AbstractUpdateRegistryPtr(
        const QString&,
        detail::data_parser::UpdatesMetaData metaData,
        detail::CustomizationVersionToUpdate customizationVersionToUpdate)>;
using EmptyUpdateRegistryFactoryFunction = nx::utils::MoveOnlyFunc<AbstractUpdateRegistryPtr()>;

class NX_UPDATE_API UpdateRegistryFactory
{
public:
    static AbstractUpdateRegistryPtr create(
        const QString& baseUrl,
        detail::data_parser::UpdatesMetaData metaData,
        detail::CustomizationVersionToUpdate customizationVersionToUpdate);
    static AbstractUpdateRegistryPtr create();
    static void setFactoryFunction(UpdateRegistryFactoryFunction factoryFunction);
    static void setEmptyFactoryFunction(EmptyUpdateRegistryFactoryFunction factoryFunction);
private:
    static UpdateRegistryFactoryFunction m_factoryFunction;
    static EmptyUpdateRegistryFactoryFunction m_emptyFactoryFunction;
};

} // namespace info
} // namespace update
} // namespace nx
