// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/reflect/instrument.h>

#include "../http_types.h"

class SettingsReader;

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

    /**
     * Set "reusePort" flag on every listening socket.
     */
    bool reusePort = false;

    /**
     * Total number of listening sockets to create.
     * E.g., if a single port was specified in Settings::endpoints and this was set to 3,
     * then 3 listening sockets are created which share the specified port.
     * All created sockets are assigned to different AIO threads when possible.
     * So, if more than one listening socket is needed, then reusePort flag is set
     * on each socket regardless of the Settings::reusePort value.
     * If 0, then it is set to std::thread::hardware_concurrency().
     */
    unsigned int listeningConcurrency = 1;

    Ssl ssl;

    /**
     * HTTP headers to be included in every response sent by the server. They are parsed
     * under the "<groupName>/extraResponseHeaders/<headerName>" section of the settings, e.g.
     * "http/extraResponseHeaders/Server" will add a "Server" header.
     */
    HttpHeaders extraResponseHeaders;

    void load(const SettingsReader& settings, const char * groupName = "http");

private:
    void loadEndpoints(
        const SettingsReader& settings,
        const char* paramFullName,
        std::vector<SocketAddress>* endpoints);

    void loadSsl(const SettingsReader& settings);
    void loadHeaders(const SettingsReader& settings);
};

NX_REFLECTION_INSTRUMENT(Settings::Ssl,
    (endpoints)(certificatePath)(certificateMonitorTimeout)(allowedSslVersions))

NX_REFLECTION_INSTRUMENT(Settings,
    (tcpBacklogSize)(connectionInactivityPeriod)(endpoints)(serverName)\
    (redirectHttpToHttps)(reusePort)(listeningConcurrency)(ssl))

} // namespace nx::network::http::server
