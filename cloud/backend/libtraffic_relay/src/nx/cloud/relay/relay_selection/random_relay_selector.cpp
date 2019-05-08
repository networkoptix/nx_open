#include "StdAfx.h"
#include "random_relay_selector.h"

namespace nx::cloud::relay {

std::string RandomRelaySelector::selectRelay(
    const std::vector<std::string>& relays) const
{
    if (relays.empty())
        return {};

    return relays[rand() % relays.size()];
}

} // namespace nx::cloud::relay
