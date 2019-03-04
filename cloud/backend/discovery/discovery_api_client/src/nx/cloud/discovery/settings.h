#pragma once

#include <nx/utils/url.h>

class QnSettings;

namespace nx::cloud::discovery {

class NX_DISCOVERY_CLIENT_API Settings
{
public:
    /**
     * The base url that the discovery client uses to connect to the discovery service.
     * The default value is https://discovery.nxvms.com
     */
    nx::utils::Url discoveryServiceUrl;

    /**
     * When calculating how long to wait before sending a registration request, the DiscoveryClient
     * adds this value to the amount of time that a request takes to travel from
     * client to server and back, effectively making the round trip time longer.
     * The default value is 0.
     */
    std::chrono::milliseconds roundTripPadding;

public:
    Settings();

    void load(const QnSettings& settings);
};

} // namespace nx::cloud::discovery
