#include "settings.h"

#include <algorithm>

#include <nx/utils/deprecated_settings.h>
#include <nx/utils/timer_manager.h>

namespace nx::network::http::server {

static constexpr char kTcpBacklogSize[] = "tcpBacklogSize";
static constexpr char kConnectionInactivityPeriod[] = "connectionInactivityPeriod";
static constexpr char kEndpointsToListen[] = "endpoints";
static constexpr char kSslEndpointsToListen[] = "sslEndpoints";

Settings::Settings(const char* groupName):
    m_groupName(groupName)
{
}

void Settings::load(const QnSettings& settings)
{
    using namespace std::chrono;

    tcpBacklogSize = settings.value(
        lm("%1/%2").args(m_groupName, kTcpBacklogSize),
        kDefaultTcpBacklogSize).toInt();

    connectionInactivityPeriod = duration_cast<seconds>(
        nx::utils::parseTimerDuration(
            settings.value(lm("%1/%2").args(m_groupName, kConnectionInactivityPeriod)).toString(),
            kDefaultConnectionInactivityPeriod));

    loadEndpoints(settings, kEndpointsToListen, &endpoints);
    loadEndpoints(settings, kSslEndpointsToListen, &sslEndpoints);
}

void Settings::loadEndpoints(
    const QnSettings& settings,
    const char* paramName,
    std::vector<SocketAddress>* endpoints)
{
    const QStringList& httpAddrToListenStrList =
        settings.value(lm("%1/%2").args(m_groupName, paramName))
            .toString().split(',', QString::SkipEmptyParts);
    std::transform(
        httpAddrToListenStrList.begin(),
        httpAddrToListenStrList.end(),
        std::back_inserter(*endpoints),
        [](const QString& str) { return network::SocketAddress(str); });
}

} // namespace nx::network::http::server
