#pragma once

#include <nx/utils/url.h>

class QnSettings;

namespace nx::cloud::discovery {

struct NX_DISCOVERY_CLIENT_API Settings
{
    /**
     * The base url that the discovery client uses to connect to the discovery service.
     * The default value is https://discovery.nxvms.com
     */
    nx::utils::Url discoveryServiceUrl;

    /**
     * When calculating how long to wait before sending a registration request, the DiscoveryClient
     * adds this value to the amount of time that a request takes to travel from
     * client to server and back, effectively making the round trip time longer.
     * The default value is 3 seconds, or 3,000 milliseconds.
     */
    std::chrono::milliseconds roundTripPadding;

    /**
     * The amount of time to wait before resending a registration request in the event of an error.
     * The default value is 1 minute, or 60,000 milliseconds.
     */
    std::chrono::milliseconds registrationErrorDelay;

    /**
     * The amount of time to wait before sending a request for online nodes after receiving
     * a response from the previous request.
     * the default value is 30 seconds, or 30,000 milliseconds.
     */
    std::chrono::milliseconds onlineNodesRequestDelay;

    Settings();

    void load(const QnSettings& settings);
};

} // namespace nx::cloud::discovery
