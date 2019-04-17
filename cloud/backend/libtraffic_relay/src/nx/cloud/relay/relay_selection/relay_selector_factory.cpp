#include "StdAfx.h"

#include "relay_selector_factory.h"

#include "random_relay_selector.h"

namespace nx::cloud::relay {

RelaySelectorFactory::RelaySelectorFactory():
    base_type(std::bind(
        &RelaySelectorFactory::defaultFactoryFunc,
        this,
        std::placeholders::_1))
{
}

RelaySelectorFactory& RelaySelectorFactory::instance()
{
    static RelaySelectorFactory instance;
    return instance;
}

std::unique_ptr<AbstractRelaySelector>
RelaySelectorFactory::defaultFactoryFunc(const conf::Settings& /*settings*/)
{
    return std::make_unique<RandomRelaySelector>();
}

} // namespace nx::cloud::relay


