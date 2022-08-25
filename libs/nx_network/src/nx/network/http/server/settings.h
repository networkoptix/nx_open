// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <nx/reflect/instrument.h>
#include <nx/network/socket_common.h>

class QnSettings;

namespace nx::network::http::server {

class NX_NETWORK_API Settings
{
public:
    /**
     * These settings are loaded from "ssl" subgroup. E.g, "http/ssl/certificatePath".
     */
    struct Ssl
    {
        std::vector<SocketAddress> endpoints;

        /**
         * Path to a certificate file.
         */
        std::string certificatePath;

        /**
         * If specified, the certificate file is checked for updates once per this interval.
         */
        std::optional<std::chrono::milliseconds> certificateMonitorTimeout;

        /**
         * Specifies SSL/TLS protocol versions to enable.
         * The format is defined by nx::network::ssl::Context::setAllowedServerVersions.
         * The empty value leaves the default protocol set.
         */
        std::string allowedSslVersions;
    };

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
    std::string serverName;
    bool redirectHttpToHttps = false;

    Ssl ssl;

    Settings(const char* groupName = "http");

    void load(const QnSettings& settings);

private:
    std::string m_groupName;

    void loadEndpoints(
        const QnSettings& settings,
        const std::string_view& paramFullName,
        std::vector<SocketAddress>* endpoints);

    void loadSsl(const QnSettings& settings);
};

NX_REFLECTION_INSTRUMENT(Settings::Ssl,
    (endpoints)(certificatePath)(certificateMonitorTimeout)(allowedSslVersions))

NX_REFLECTION_INSTRUMENT(Settings,
    (tcpBacklogSize)(connectionInactivityPeriod)(endpoints)(serverName)(redirectHttpToHttps)(ssl))

} // namespace nx::network::http::server
