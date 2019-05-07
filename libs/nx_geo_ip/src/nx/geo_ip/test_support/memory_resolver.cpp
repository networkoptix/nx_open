#include "memory_resolver.h"

namespace nx::geo_ip::test {

namespace {

static std::string toString (const nx::network::SocketAddress& endpoint)
{
    return endpoint.toString().replace("localhost", "127.0.0.1").toStdString();
}

}

void MemoryResolver::add(const nx::network::SocketAddress& endpoint, const Location& location)
{
    m_endpointLocations.emplace(toString(endpoint), location);
}

Result MemoryResolver::resolve(const nx::network::SocketAddress& endpoint)
{
    auto it = m_endpointLocations.find(toString(endpoint));
    if (it == m_endpointLocations.end())
        return {ResultCode::notFound, {}};
    return {ResultCode::ok, it->second};
}

} // namespace nx::geo_ip::test