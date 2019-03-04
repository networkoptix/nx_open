#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::cloud::discovery {

namespace {

static const QLatin1String kRoundTripPadding("discovery/roundTripPadding");
static const std::chrono::milliseconds kDefaultRoundTripPadding(0);

static const QLatin1String kDiscoveryServiceUrl("discovery/discoveryServiceUrl");
static const QLatin1String kDefaultDiscoveryServiceUrl("https://discovery.nxvms.com");

} // namespace

Settings::Settings():
    discoveryServiceUrl(kDefaultDiscoveryServiceUrl),
    roundTripPadding(kDefaultRoundTripPadding)
{
}

void Settings::load(const QnSettings& settings)
{
    discoveryServiceUrl =
        settings.value(kDiscoveryServiceUrl, kDefaultDiscoveryServiceUrl).toString();

    roundTripPadding = std::chrono::milliseconds(
        settings.value(kRoundTripPadding, kDefaultRoundTripPadding.count()).toInt());
}

} // namespace nx::cloud::discovery