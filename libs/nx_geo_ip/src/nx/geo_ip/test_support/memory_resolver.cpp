#include "memory_resolver.h"

namespace nx::geo_ip::test {

void MemoryResolver::add(const std::string& ipAddress, const Location& location)
{
    m_endpointLocations.emplace(ipAddress, location);
}

Location MemoryResolver::resolve(const std::string& ipAddress)
{
    auto it = m_endpointLocations.find(ipAddress);
    if (it == m_endpointLocations.end())
        throw Exception(ResultCode::notFound, "ip address: " + ipAddress + " not found");
    return it->second;
}

} // namespace nx::geo_ip::test
