#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::cloud::discovery {

namespace {

static const QLatin1String kRoundTripPadding("discovery/roundTripPadding");
static const std::chrono::milliseconds kDefaultRoundTripPadding(0);

static const QLatin1String kDiscoveryServiceUrl("discovery/discoveryServiceUrl");
static const QLatin1String kDefaultDiscoveryServiceUrl("https://discovery.nxvms.com");

static const QLatin1String kRegistrationErrorDelay("discovery/registrationErrorDelay");
static const std::chrono::milliseconds kDefaultRegistrationErrorDelay =
    std::chrono::minutes(1);

static const QLatin1String kOnlineNodesRequestDelay("discovery/onlineNodesRequestDelay");
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
    discoveryServiceUrl =
        settings.value(kDiscoveryServiceUrl, kDefaultDiscoveryServiceUrl).toString();

    roundTripPadding = std::chrono::milliseconds(
        settings.value(kRoundTripPadding, kDefaultRoundTripPadding.count()).toInt());

    registrationErrorDelay = std::chrono::milliseconds(
        settings.value(kRegistrationErrorDelay, kDefaultRegistrationErrorDelay.count()).toInt());

    onlineNodesRequestDelay = std::chrono::milliseconds(
        settings.value(kOnlineNodesRequestDelay, kDefaultOnlineNodesRequestDelay.count()).toInt());
}

} // namespace nx::cloud::discovery