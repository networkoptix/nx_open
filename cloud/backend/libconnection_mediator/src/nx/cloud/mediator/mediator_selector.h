#pragma once

#include <vector>

#include <nx/utils/basic_factory.h>

#include "mediator_endpoint.h"

namespace nx::hpm {

namespace conf { class Settings; }

class AbstractMediatorSelector
{
public:
    virtual ~AbstractMediatorSelector() = default;
    virtual MediatorEndpoint select(
        const std::vector<MediatorEndpoint>& endpoints) const = 0;
};

class MediatorSelector : public AbstractMediatorSelector
{
    virtual MediatorEndpoint select(
        const std::vector<MediatorEndpoint>& endpoints) const override;
};

using MediatorSelectorFactoryFunction =
    std::unique_ptr<AbstractMediatorSelector>(const conf::Settings&);

class MediatorSelectorFactory :
    public nx::utils::BasicFactory<MediatorSelectorFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<MediatorSelectorFactoryFunction>;

public:
    MediatorSelectorFactory();

    static MediatorSelectorFactory& instance();

private:
    std::unique_ptr<AbstractMediatorSelector> defaultFactoryFunction(
        const conf::Settings& settings);
};

} // namespace nx::hpm
