#pragma once

#include <memory>

#include <nx/utils/basic_factory.h>

#include "abstract_relay_selector.h"

namespace nx::cloud::relay {

namespace conf { class Settings; }

using RelaySelectorFunc = std::unique_ptr<AbstractRelaySelector>(const conf::Settings&);

class RelaySelectorFactory:
    public nx::utils::BasicFactory<RelaySelectorFunc>
{
    using base_type = BasicFactory<RelaySelectorFunc>;

public:
    RelaySelectorFactory(const RelaySelectorFactory&) = delete;
    RelaySelectorFactory(RelaySelectorFactory&&) = delete;
    RelaySelectorFactory& operator=(const RelaySelectorFactory&) = delete;

public:
    RelaySelectorFactory();
    static RelaySelectorFactory &instance();

private:
    std::unique_ptr<AbstractRelaySelector> defaultFactoryFunc(const conf::Settings& settings);
};

} // namespace nx::cloud::relay
