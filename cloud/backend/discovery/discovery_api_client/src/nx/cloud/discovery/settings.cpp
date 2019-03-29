#include "settings.h"

#include <nx/utils/timer_manager.h>
#include <nx/utils/deprecated_settings.h>

namespace nx::cloud::discovery {

namespace {

static constexpr char kDiscoveryEnabled[] = "discovery/enabled";
static constexpr char kDefaultDiscoveryEnabled[] = "false";

static constexpr char kDiscoveryServiceUrl[] = "discovery/discoveryServiceUrl";
static constexpr char kDefaultDiscoveryServiceUrl[] = "https://discovery.vmsproxy.com";

static constexpr char kRoundTripPadding[] = "discovery/roundTripPadding";
static const std::chrono::milliseconds kDefaultRoundTripPadding =
    std::chrono::seconds(3);

static constexpr char kRegistrationErrorDelay[] = "discovery/registrationErrorDelay";
static const std::chrono::milliseconds kDefaultRegistrationErrorDelay =
    std::chrono::minutes(1);

static constexpr char kOnlineNodesRequestDelay[] = "discovery/onlineNodesRequestDelay";
static const std::chrono::milliseconds kDefaultOnlineNodesRequestDelay =
    std::chrono::seconds(30);

} // namespace

Settings::Settings():
    discoveryServiceUrl(kDefaultDiscoveryServiceUrl),
    roundTripPadding(kDefaultRoundTripPadding),
    registrationErrorDelay(kDefaultRegistrationErrorDelay),
    onlineNodesRequestDelay(kDefaultOnlineNodesRequestDelay)
{
}

void Settings::load(const QnSettings& settings)
{
    enabled = settings.value(kDiscoveryEnabled, kDefaultDiscoveryEnabled).toString() == "true";

    discoveryServiceUrl =
        settings.value(kDiscoveryServiceUrl, kDefaultDiscoveryServiceUrl).toString();

    roundTripPadding = nx::utils::parseTimerDuration(
        settings.value(kRoundTripPadding).toString(),
        kDefaultRoundTripPadding);

    registrationErrorDelay = nx::utils::parseTimerDuration(
        settings.value(kRegistrationErrorDelay).toString(),
        kDefaultRegistrationErrorDelay);

    onlineNodesRequestDelay = nx::utils::parseTimerDuration(
        settings.value(kOnlineNodesRequestDelay).toString(),
        kDefaultOnlineNodesRequestDelay);
}

} // namespace nx::cloud::discovery