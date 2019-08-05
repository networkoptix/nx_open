#pragma once

#include <nx/utils/basic_factory.h>

namespace nx::cloud::relay {

class AbstractRelaySelector
{
public:
    virtual ~AbstractRelaySelector() = default;
    virtual std::string selectRelay(const std::vector<std::string>& relays) const = 0;
};

} // namespace nx::cloud::relay
