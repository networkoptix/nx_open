#pragma once

#include <chrono>
#include <vector>

#include <nx/network/socket_common.h>

class QnSettings;

namespace nx::network::http::server {

class NX_NETWORK_API Settings
{
public:
    static constexpr int kDefaultTcpBacklogSize = 128;
    static constexpr std::chrono::milliseconds kDefaultConnectionInactivityPeriod =
        std::chrono::hours(2);

    /**
     * Backlog value to pass to tcpServerSocket->listen call.
     */
    int tcpBacklogSize = kDefaultTcpBacklogSize;
    std::chrono::milliseconds connectionInactivityPeriod =
        kDefaultConnectionInactivityPeriod;
    std::vector<SocketAddress> endpoints;
    std::vector<SocketAddress> sslEndpoints;

    Settings(const char* groupName = "http");

    void load(const QnSettings& settings);

private:
    std::string m_groupName;

    void loadEndpoints(
        const QnSettings& settings,
        const char* paramName,
        std::vector<SocketAddress>* endpoints);
};

} // namespace nx::network::http::server
