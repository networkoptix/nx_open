// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_api_notifications.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/uuid.h>

namespace nx::cloud::relay::api {

static constexpr char kClientEndpoint[] = "X-Nx-Client-Endpoint";
static constexpr char kOpenTunnel[] = "OPEN_TUNNEL";

void OpenTunnelNotification::setClientPeerName(std::string name)
{
    m_clientPeerName = std::move(name);
}

const std::string& OpenTunnelNotification::clientPeerName() const
{
    return m_clientPeerName;
}

void OpenTunnelNotification::setClientEndpoint(network::SocketAddress endpoint)
{
    m_clientEndpoint = std::move(endpoint);
}

const network::SocketAddress& OpenTunnelNotification::clientEndpoint() const
{
    return m_clientEndpoint;
}

void OpenTunnelNotification::setConnectionTestRequested(bool requested)
{
    m_isConnectionTestRequested = requested;
}

bool OpenTunnelNotification::connectionTestRequested() const
{
    return m_isConnectionTestRequested;
}

nx::network::http::Message OpenTunnelNotification::toHttpMessage() const
{
    nx::network::http::Message message(nx::network::http::MessageType::request);
    message.request->requestLine.method = kOpenTunnel;
    message.request->requestLine.version = {kRelayProtocolName, kRelayProtocolVersion};
    message.request->requestLine.url = nx::network::url::joinPath(
        nx::cloud::relay::api::kRelayClientPathPrefix,
        m_clientPeerName);
    if (m_isConnectionTestRequested)
        message.request->requestLine.url.setQuery("connection-test=1");
    message.request->headers.emplace(
        kClientEndpoint, m_clientEndpoint.toString());
    return message;
}

bool OpenTunnelNotification::parse(const nx::network::http::Message& message)
{
    if (message.type != nx::network::http::MessageType::request)
        return false;

    if (message.request->requestLine.method != kOpenTunnel)
        return false;

    auto path = message.request->requestLine.url.path().toStdString();
    if (!nx::utils::startsWith(path, nx::cloud::relay::api::kRelayClientPathPrefix))
        return false;
    path.erase(0, strlen(nx::cloud::relay::api::kRelayClientPathPrefix));
    m_clientPeerName = path;

    auto clientEndpointIter = message.request->headers.find(kClientEndpoint);
    if (clientEndpointIter == message.request->headers.end())
        return false;
    m_clientEndpoint = network::SocketAddress(clientEndpointIter->second);

    auto connectionTest = message.request->requestLine.url.queryItem("connection-test");
    if (!connectionTest.isEmpty() && connectionTest != "0" &&
        nx::utils::stricmp(connectionTest.toStdString(), "false") != 0)
        m_isConnectionTestRequested = true;

    return true;
}

//-------------------------------------------------------------------------------------------------

static constexpr char kKeepAlive[] = "KEEP_ALIVE";
static constexpr char kConnectionPath[] = "connection";

nx::network::http::Message KeepAliveNotification::toHttpMessage() const
{
    nx::network::http::Message message(nx::network::http::MessageType::request);
    message.request->requestLine.method = kKeepAlive;
    message.request->requestLine.version = {kRelayProtocolName, kRelayProtocolVersion};
    message.request->requestLine.url = nx::network::url::joinPath(
        nx::cloud::relay::api::kRelayClientPathPrefix,
        kConnectionPath);
    return message;
}

bool KeepAliveNotification::parse(const nx::network::http::Message& message)
{
    if (message.type != nx::network::http::MessageType::request)
        return false;

    if (message.request->requestLine.method != kKeepAlive)
        return false;

    if (message.request->requestLine.url.path().toStdString() !=
        nx::network::url::joinPath(
            nx::cloud::relay::api::kRelayClientPathPrefix,
            kConnectionPath))
    {
        return false;
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

static constexpr char kTestConnectionPath[] = "test-connection";
static constexpr char kTestIdHeader[] = "X-Nx-Connection-Test-Id";
static constexpr char kTestIdResponseHeader[] = "X-Nx-Connection-Test-Id-Response";

TestConnectionNotification::TestConnectionNotification()
    :
    m_testId(nx::Uuid::createUuid().toSimpleStdString())
{
}

TestConnectionNotification::TestConnectionNotification(std::string host)
    :
    m_host(std::move(host)),
    m_testId(nx::Uuid::createUuid().toSimpleStdString())
{
}

void TestConnectionNotification::setTestId(std::string id)
{
    m_testId = std::move(id);
}

const std::string& TestConnectionNotification::testId() const
{
    return m_testId;
}

nx::network::http::Message TestConnectionNotification::toHttpMessage() const
{
    nx::network::http::Message message(nx::network::http::MessageType::request);
    message.request->requestLine.method = kHttpMethod;
    message.request->requestLine.version = nx::network::http::http_1_1;
    message.request->requestLine.url = nx::network::url::joinPath(
        nx::cloud::relay::api::kRelayClientPathPrefix,
        kTestConnectionPath);
    nx::network::http::insertOrReplaceHeader(
        &message.request->headers,
        nx::network::http::HttpHeader("Host", m_host));
    nx::network::http::insertOrReplaceHeader(
        &message.request->headers,
        nx::network::http::HttpHeader(kTestIdHeader, m_testId));
    return message;
}

bool TestConnectionNotification::parse(const nx::network::http::Message& message)
{
    if (message.type != nx::network::http::MessageType::request)
        return false;

    if (message.request->requestLine.method != kHttpMethod)
        return false;

    if (message.request->requestLine.url.path().toStdString() !=
        nx::network::url::joinPath(
            nx::cloud::relay::api::kRelayClientPathPrefix,
            kTestConnectionPath))
    {
        return false;
    }

    if (auto val = nx::network::http::getHeaderValue(message.request->headers, kTestIdHeader);
        val.empty())
    {
        return false;
    }
    else
    {
        m_testId = std::move(val);
    }

    return true;
}

TestConnectionNotificationResponse::TestConnectionNotificationResponse(
    nx::network::http::StatusCode::Value code)
    :
    m_code(code)
{
}

void TestConnectionNotificationResponse::setStatusCode(nx::network::http::StatusCode::Value code)
{
    m_code = code;
}

nx::network::http::StatusCode::Value TestConnectionNotificationResponse::statusCode() const
{
    return m_code;
}

void TestConnectionNotificationResponse::setTestId(std::string id)
{
    m_testId = std::move(id);
}

const std::string& TestConnectionNotificationResponse::testId() const
{
    return m_testId;
}

nx::network::http::Message TestConnectionNotificationResponse::toHttpMessage() const
{
    nx::network::http::Message message(nx::network::http::MessageType::response);
    message.response->statusLine.statusCode = m_code;
    message.response->statusLine.version = nx::network::http::http_1_1;
    nx::network::http::insertOrReplaceHeader(
        &message.response->headers,
        nx::network::http::HttpHeader(kTestIdResponseHeader, m_testId));
    return message;
}

bool TestConnectionNotificationResponse::parse(const nx::network::http::Message& message)
{
    if (message.type != nx::network::http::MessageType::response)
        return false;

    if (auto val = nx::network::http::getHeaderValue(
            message.response->headers,
            kTestIdResponseHeader);
        val.empty())
    {
        return false;
    }
    else
    {
        m_testId = std::move(val);
    }

    m_code = message.response->statusLine.statusCode;

    return true;
}

} // namespace nx::cloud::relay::api
