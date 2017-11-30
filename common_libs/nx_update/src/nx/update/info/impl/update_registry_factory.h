#pragma once

#include <memory>
#include <nx/update/info/abstract_update_registry.h>
#include <nx/utils/move_only_func.h>

namespace nx {
namespace update {
namespace info {
namespace impl {

using AbstractUpdateRegistryPtr = std::unique_ptr<AbstractUpdateRegistry>;
using UpdateRegistryFactoryFunction = nx::utils::MoveOnlyFunc<AbstractUpdateRegistryPtr()>;

class UpdateRegistryFactory
{
public:
    static AbstractUpdateRegistryPtr create();
    static void setFactoryFunction(UpdateRegistryFactoryFunction factoryFunction);
private:
    static UpdateRegistryFactoryFunction m_factoryFunction;
};

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
