#pragma once

#include "abstract_resolver.h"

namespace nx {
namespace network {

/**
 * Resolves address using OS-provided API.
 */
class NX_NETWORK_API SystemResolver:
    public AbstractResolver
{
public:
    virtual std::deque<HostAddress> resolve(const QString& hostName, int ipVersion) override;
};

} // namespace network
} // namespace nx
