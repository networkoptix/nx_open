#pragma once

#include "nx/geo_ip/abstract_resolver.h"

#include <map>

namespace nx::geo_ip::test {

class NX_GEO_IP_API MemoryResolver: public AbstractResolver
{
public:
    virtual ~MemoryResolver() override = default;

    void add(const nx::network::SocketAddress& endpoint, const Location& location);

    virtual Result resolve(const nx::network::SocketAddress& endpoint) override;

private:
    std::map<std::string, Location> m_endpointLocations;
};

} // namespace nx::geo_ip::test