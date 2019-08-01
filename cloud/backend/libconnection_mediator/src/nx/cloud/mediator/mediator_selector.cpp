#include "mediator_selector.h"

#include <nx/utils/random.h>

#include "settings.h"

namespace nx::hpm {

MediatorEndpoint MediatorSelector::select(const std::vector<MediatorEndpoint>& endpoints) const
{
    if (endpoints.empty())
        return {};
    return nx::utils::random::choice(endpoints);
}

//-------------------------------------------------------------------------------------------------
// MediatorSelectorFactory

MediatorSelectorFactory::MediatorSelectorFactory():
    base_type(std::bind(&MediatorSelectorFactory::defaultFactoryFunction,
        this, std::placeholders::_1))
{
}

MediatorSelectorFactory& MediatorSelectorFactory::instance()
{
    static MediatorSelectorFactory factory;
    return factory;
}

std::unique_ptr<AbstractMediatorSelector>
MediatorSelectorFactory::defaultFactoryFunction(const conf::Settings& /*settings*/)
{
    return std::make_unique<MediatorSelector>();
}

} // namespace nx::hpm