#include "mediator_endpoint.h"

namespace nx::hpm {

std::string MediatorEndpoint::toString() const
{
    std::string s("{ domainName: ");
    s += domainName;
    s += ", httpPort: ";
    s += std::to_string(httpPort);
    s += ", httpsPort: ";
    s += std::to_string(httpsPort);
    s += ", stunUdpPort: ";
    s += std::to_string(stunUdpPort);
    s += " }";
    return s;
}

bool MediatorEndpoint::operator==(const MediatorEndpoint& other) const
{
    return
        domainName == other.domainName
        && httpPort == other.httpPort
        && httpsPort == other.httpsPort
        && stunUdpPort == other.stunUdpPort;
}

bool MediatorEndpoint::operator !=(const MediatorEndpoint& other) const
{
    return !(*this == other);
}

} // namespace nx::hpm