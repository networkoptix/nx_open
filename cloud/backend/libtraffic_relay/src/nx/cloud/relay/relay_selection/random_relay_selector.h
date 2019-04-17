#pragma once

#include "abstract_relay_selector.h"

namespace nx::cloud::relay {

class RandomRelaySelector:
    public AbstractRelaySelector
{
public:
    virtual std::string selectRelay(const std::vector<std::string>& relays) const override;
};

} // namespace nx::cloud::relay