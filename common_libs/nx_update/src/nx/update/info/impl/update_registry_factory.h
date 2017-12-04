#pragma once

#include <memory>
#include <nx/update/info/abstract_update_registry.h>
#include <nx/utils/move_only_func.h>
#include <nx/update/info/detail/data_parser/updates_meta_data.h>
#include <nx/update/info/detail/customization_version_data.h>

namespace nx {
namespace update {
namespace info {
namespace impl {

using AbstractUpdateRegistryPtr = std::unique_ptr<AbstractUpdateRegistry>;
using UpdateRegistryFactoryFunction = nx::utils::MoveOnlyFunc<AbstractUpdateRegistryPtr()>;

class NX_UPDATE_API UpdateRegistryFactory
{
public:
    static AbstractUpdateRegistryPtr create(
        detail::data_parser::UpdatesMetaData metaData,
        detail::CustomizationVersionToUpdate customizationVersionToUpdate);
    static void setFactoryFunction(UpdateRegistryFactoryFunction factoryFunction);
private:
    static UpdateRegistryFactoryFunction m_factoryFunction;
};

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
