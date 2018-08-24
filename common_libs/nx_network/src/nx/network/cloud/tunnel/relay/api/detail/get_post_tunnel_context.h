#pragma once

#include <memory>

#include <nx/network/http/http_async_client.h>
#include <nx/utils/url.h>

#include "../relay_api_client.h"

namespace nx::cloud::relay::api::detail {

class TunnelContext
{
public:
    QUrl tunnelUrl;
    std::unique_ptr<nx_http::AsyncClient> httpClient;
    std::unique_ptr<AbstractStreamSocket> connection;
    nx::Buffer serializedOpenUpChannelRequest;

    virtual ~TunnelContext() = default;

    virtual bool parseOpenDownChannelResponse(
        const nx_http::Response& response) = 0;

    virtual void invokeUserHandler(api::ResultCode resultCode) = 0;
};

//-------------------------------------------------------------------------------------------------

class ServerTunnelContext:
    public TunnelContext
{
public:
    ServerTunnelContext(BeginListeningHandler userHandler);

    virtual bool parseOpenDownChannelResponse(
        const nx_http::Response& response) override;

    virtual void invokeUserHandler(api::ResultCode resultCode) override;

private:
    BeginListeningResponse m_beginListeningResponse;
    BeginListeningHandler m_userHandler;
};

//-------------------------------------------------------------------------------------------------

class ClientTunnelContext:
    public TunnelContext
{
public:
    ClientTunnelContext(OpenRelayConnectionHandler userHandler);

    virtual bool parseOpenDownChannelResponse(
        const nx_http::Response& response) override;

    virtual void invokeUserHandler(api::ResultCode resultCode) override;

private:
    OpenRelayConnectionHandler m_userHandler;
};

} // namespace nx::cloud::relay::api::detail
