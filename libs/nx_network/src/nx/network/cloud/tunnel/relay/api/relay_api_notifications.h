// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/buffer.h>

namespace nx::cloud::relay::api {

class NX_NETWORK_API OpenTunnelNotification
{
public:
    static constexpr std::string_view kHttpMethod = "OPEN_TUNNEL";

    void setClientPeerName(std::string name);
    const std::string& clientPeerName() const;

    void setClientEndpoint(network::SocketAddress endpoint);
    const network::SocketAddress& clientEndpoint() const;

    void setConnectionTestRequested(bool requested = true);
    bool connectionTestRequested() const;

    nx::network::http::Message toHttpMessage() const;
    bool parse(const nx::network::http::Message& message);

private:
    std::string m_clientPeerName;
    network::SocketAddress m_clientEndpoint;
    bool m_isConnectionTestRequested = false;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API KeepAliveNotification
{
public:
    static constexpr std::string_view kHttpMethod = "KEEP_ALIVE";

    nx::network::http::Message toHttpMessage() const;

    /**
     * Example: "UPDATE /relay/client/connection NXRELAY/0.1".
     */
    bool parse(const nx::network::http::Message& message);
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API TestConnectionNotification
{
public:
    static constexpr std::string_view kHttpMethod = "GET";
    static constexpr std::string_view kTestIdResponseHeader = "X-Nx-Connection-Test-Id-Response";

    TestConnectionNotification();
    explicit TestConnectionNotification(std::string host);

    void setTestId(std::string id);
    const std::string& testId() const;

    nx::network::http::Message toHttpMessage() const;
    bool parse(const nx::network::http::Message& message);

private:
    std::string m_host;
    std::string m_testId;
};

class NX_NETWORK_API TestConnectionNotificationResponse
{
public:
    explicit TestConnectionNotificationResponse(
        nx::network::http::StatusCode::Value code = nx::network::http::StatusCode::ok);

    void setStatusCode(nx::network::http::StatusCode::Value code);
    nx::network::http::StatusCode::Value statusCode() const;

    void setTestId(std::string id);
    const std::string& testId() const;

    nx::network::http::Message toHttpMessage() const;
    bool parse(const nx::network::http::Message& message);

private:
    nx::network::http::StatusCode::Value m_code;
    std::string m_testId;
};

} // namespace nx::cloud::relay::api
