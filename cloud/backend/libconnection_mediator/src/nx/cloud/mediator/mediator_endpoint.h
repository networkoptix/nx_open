#pragma once

#include <string>

namespace nx::hpm {

/**
 * Represents the endpoint that a mediator instance listens on,
 * including http(or https) and stun udp ports. A given port is set to -1 if unused.
 */
struct MediatorEndpoint
{
    static constexpr int kPortUnused = -1;

    std::string domainName; //< Ip address without port, or domain name to be resolved by dns
    int httpPort = kPortUnused;
    int httpsPort = kPortUnused;
    int stunUdpPort = kPortUnused;

    std::string toString() const;
    bool operator ==(const MediatorEndpoint& other) const;
    bool operator !=(const MediatorEndpoint& other) const;
};

}