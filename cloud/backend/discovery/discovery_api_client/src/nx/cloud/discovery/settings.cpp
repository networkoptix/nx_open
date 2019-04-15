#include "settings.h"

#include <nx/utils/timer_manager.h>
#include <nx/utils/deprecated_settings.h>

namespace nx::cloud::discovery {

namespace {

static constexpr char kDiscoveryEnabled[] = "enabled";
static constexpr char kDefaultDiscoveryEnabled[] = "false";

static constexpr char kDiscoveryServiceUrl[] = "discoveryServiceUrl";
static constexpr char kDefaultDiscoveryServiceUrl[] = "https://discovery.vmsproxy.com";

static constexpr char kRoundTripPadding[] = "roundTripPadding";
static const std::chrono::milliseconds kDefaultRoundTripPadding =
    std::chrono::seconds(3);

static constexpr char kRegistrationErrorDelay[] = "registrationErrorDelay";
static const std::chrono::milliseconds kDefaultRegistrationErrorDelay =
    std::chrono::minutes(1);

static constexpr char kOnlineNodesRequestDelay[] = "onlineNodesRequestDelay";
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

void Settings::load(const QnSettings& settings, std::string groupName)
{
    while (!groupName.empty() && groupName.back() == '/')
        groupName.pop_back();

    QString settingsTemplate = groupName.empty() ? "%1%2" : "%1/%2";

    enabled = settings.value(
        lm(settingsTemplate).arg(groupName).arg(kDiscoveryEnabled),
        kDefaultDiscoveryEnabled).toString() == "true";

    discoveryServiceUrl = settings.value(
        lm(settingsTemplate).arg(groupName).arg(kDiscoveryServiceUrl),
        kDefaultDiscoveryServiceUrl).toString();

    roundTripPadding = nx::utils::parseTimerDuration(
        settings.value(lm(settingsTemplate).arg(groupName).arg(kRoundTripPadding)).toString(),
        kDefaultRoundTripPadding);

    registrationErrorDelay = nx::utils::parseTimerDuration(
        settings.value(lm(settingsTemplate).arg(groupName).arg(kRegistrationErrorDelay)).toString(),
        kDefaultRegistrationErrorDelay);

    onlineNodesRequestDelay = nx::utils::parseTimerDuration(
        settings.value(lm(settingsTemplate).arg(groupName).arg(kOnlineNodesRequestDelay)).toString(),
        kDefaultOnlineNodesRequestDelay);
}

} // namespace nx::cloud::discovery