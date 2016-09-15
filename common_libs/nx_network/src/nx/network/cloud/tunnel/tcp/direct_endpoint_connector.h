/**********************************************************
* Jul 7, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <list>

#include <nx/network/http/asynchttpclient.h>
#include <nx/network/system_socket.h>
#include "../abstract_tunnel_connector.h"

#define DO_MSERVER_VERIFICATION

namespace nx {
namespace network {
namespace cloud {
namespace tcp {

/**
    Currently, this class tests that tcp-connection can be established to reported endpoint.
    No remote peer validation is performed!
*/
class NX_NETWORK_API DirectEndpointConnector
    :
    public AbstractTunnelConnector
{
public:
    DirectEndpointConnector(
        AddressEntry targetHostAddress,
        nx::String connectSessionId);
    virtual ~DirectEndpointConnector();

    virtual void stopWhileInAioThread() override;

    virtual int getPriority() const override;
    virtual void connect(
        const hpm::api::ConnectResponse& response,
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) override;
    virtual const AddressEntry& targetPeerAddress() const override;

private:
    struct ConnectionContext
    {
        SocketAddress endpoint;
        #ifdef DO_MSERVER_VERIFICATION
            nx_http::AsyncHttpClientPtr httpClient;
        #else
            std::unique_ptr<TCPSocket> connection;
        #endif
    };

    const AddressEntry m_targetHostAddress;
    const nx::String m_connectSessionId;
    ConnectCompletionHandler m_completionHandler;
    std::list<ConnectionContext> m_connections;

    #ifdef DO_MSERVER_VERIFICATION
        void onHttpRequestDone(
            nx_http::AsyncHttpClientPtr httpClient,
            std::list<ConnectionContext>::iterator socketIter);
    #else
        void onConnected(
            SystemError::ErrorCode sysErrorCode,
            std::list<ConnectionContext>::iterator socketIter);
    #endif

    void reportErrorOnEndpointVerificationFailure(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode);
    bool verifyHostResponse(nx_http::AsyncHttpClientPtr httpClient);
    void reportSuccessfulVerificationResult(
        SocketAddress endpoint,
        std::unique_ptr<AbstractStreamSocket> streamSocket);
};

} // namespace tcp
} // namespace cloud
} // namespace network
} // namespace nx
