#include "settings.h"

#include <nx/utils/deprecated_settings.h>

namespace nx::cloud::discovery {

namespace {

static const QLatin1String kDiscoveryServiceUrl("discovery/discoveryServiceUrl");
static const QLatin1String kDefaultDiscoveryServiceUrl("https://discovery.nxvms.com");

static const QLatin1String kRoundTripPadding("discovery/roundTripPadding");
static const std::chrono::milliseconds kDefaultRoundTripPadding =
    std::chrono::seconds(3);

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

    // On Linux, count() returns a long, which is unsupported by QVariant constructors
    // int is supported though.
    roundTripPadding = std::chrono::milliseconds(settings.value(
        kRoundTripPadding,
        (int)kDefaultRoundTripPadding.count()).toInt());

    registrationErrorDelay = std::chrono::milliseconds(settings.value(
        kRegistrationErrorDelay,
        (int)kDefaultRegistrationErrorDelay.count()).toInt());

    onlineNodesRequestDelay = std::chrono::milliseconds(settings.value(
        kOnlineNodesRequestDelay,
        (int)kDefaultOnlineNodesRequestDelay.count()).toInt());
}

} // namespace nx::cloud::discovery