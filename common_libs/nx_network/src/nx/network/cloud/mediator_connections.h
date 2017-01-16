#pragma once

#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/stun/udp_client.h>

#include "abstract_cloud_system_credentials_provider.h"
#include "base_mediator_client.h"
#include "data/bind_data.h"
#include "data/client_bind_data.h"
#include "data/connect_data.h"
#include "data/connection_ack_data.h"
#include "data/connection_requested_event_data.h"
#include "data/connection_result_data.h"
#include "data/listen_data.h"
#include "data/ping_data.h"
#include "data/resolve_peer_data.h"
#include "data/resolve_domain_data.h"

namespace nx {
namespace hpm {
namespace api {

/**
 * Provides access to mediator functions to be used by clients.
 * @note These requests DO NOT require authentication.
 */
template<class NetworkClientType>
class MediatorClientConnection:
    public BaseMediatorClient<NetworkClientType>
{
public:
    template<typename ... Args>
    MediatorClientConnection(Args ... args):
        BaseMediatorClient<NetworkClientType>(std::forward<Args>(args) ...)
    {
    }

    template<typename Request>
    void send(Request request, utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> handler)
    {
        this->doRequest(std::move(request), std::move(handler));
    }

    void resolveDomain(
        nx::hpm::api::ResolveDomainRequest resolveData,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolveDomainResponse)> completionHandler)
    {
        this->doRequest(
            std::move(resolveData),
            std::move(completionHandler));
    }

    void resolvePeer(
        nx::hpm::api::ResolvePeerRequest resolveData,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolvePeerResponse)> completionHandler)
    {
        this->doRequest(
            std::move(resolveData),
            std::move(completionHandler));
    }

    void connect(
        nx::hpm::api::ConnectRequest connectData,
        utils::MoveOnlyFunc<void(
            stun::TransportHeader /*stunTransportHeader*/,
            nx::hpm::api::ResultCode,
            nx::hpm::api::ConnectResponse)> completionHandler)
    {
        this->doRequest(
            std::move(connectData),
            std::move(completionHandler));
    }

    void bind(
        nx::hpm::api::ClientBindRequest request,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ClientBindResponse)> completionHandler)
    {
        this->doRequest(
            std::move(request),
            std::move(completionHandler));
    }
};

typedef MediatorClientConnection<stun::AsyncClientUser> MediatorClientTcpConnection;
typedef MediatorClientConnection<stun::UdpClient> MediatorClientUdpConnection;

/**
 * Provides access to mediator functions to be used by servers.
 * @note All server requests MUST be authorized by cloudId and cloudAuthenticationKey.
 */
template<class NetworkClientType>
class MediatorServerConnection:
    public BaseMediatorClient<NetworkClientType>
{
public:
    template<typename Arg>
    MediatorServerConnection(Arg arg, AbstractCloudSystemCredentialsProvider* connector):
        BaseMediatorClient<NetworkClientType>(std::move(arg)),
        m_connector(connector)
    {
        NX_ASSERT(m_connector);
    }

    /**
     * Ask mediator to test connection to addresses.
     * @return list of endpoints available to the mediator.
     */
    void ping(
        nx::hpm::api::PingRequest requestData,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::PingResponse)> completionHandler)
    {
        this->doAuthRequest(
            stun::extension::methods::ping,
            std::move(requestData),
            std::move(completionHandler));
    }

    /**
     * Reports to mediator that local server is available on \a addresses.
     */
    void bind(
        nx::hpm::api::BindRequest requestData,
        utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        this->doAuthRequest(
            std::move(requestData),
            std::move(completionHandler));
    }

    /**
     * Notifies mediator this server is willing to accept cloud connections.
     */
    void listen(
        nx::hpm::api::ListenRequest listenParams,
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ListenResponse)> completionHandler)
    {
        this->doAuthRequest(
            std::move(listenParams),
            std::move(completionHandler));
    }

    /**
     * Server uses this request to confirm its willingness to proceed with cloud connection.
     */
    void connectionAck(
        nx::hpm::api::ConnectionAckRequest request,
        utils::MoveOnlyFunc<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        this->doAuthRequest(
            std::move(request),
            std::move(completionHandler));
    }

    String selfPeerId() const
    {
        if (m_connector)
            if (auto credentials = m_connector->getSystemCredentials())
                return credentials->hostName();

        return String();
    }

    AbstractCloudSystemCredentialsProvider* credentialsProvider() const
    {
        return m_connector;
    }

protected:
    template<typename RequestData, typename CompletionHandlerType>
    void doAuthRequest(
        RequestData requestData,
        CompletionHandlerType completionHandler)
    {
        stun::Message request(stun::Header(stun::MessageClass::request, RequestData::kMethod));
        requestData.serialize(&request);

        if (auto credentials = m_connector->getSystemCredentials())
        {
            request.newAttribute<stun::extension::attrs::SystemId>(credentials->systemId);
            request.newAttribute<stun::extension::attrs::ServerId>(credentials->serverId);
            request.insertIntegrity(credentials->systemId, credentials->key);
        }

        this->sendRequestAndReceiveResponse(
            std::move(request),
            std::move(completionHandler));
    }

private:
    AbstractCloudSystemCredentialsProvider* m_connector;
};

typedef MediatorServerConnection<stun::UdpClient> MediatorServerUdpConnection;


class MediatorServerTcpConnection:
    public MediatorServerConnection<stun::AsyncClientUser>
{
public:
    MediatorServerTcpConnection(
        std::shared_ptr<nx::stun::AbstractAsyncClient> stunClient,
        AbstractCloudSystemCredentialsProvider* connector)
    :
        MediatorServerConnection<stun::AsyncClientUser>(
            std::move(stunClient),
            connector)
    {
    }

    void setOnConnectionRequestedHandler(
        std::function<void(nx::hpm::api::ConnectionRequestedEvent)> handler)
    {
        setIndicationHandler(
            nx::stun::extension::indications::connectionRequested,
            [handler = std::move(handler)](nx::stun::Message msg)
            {
                ConnectionRequestedEvent indicationData;
                indicationData.parse(msg);
                handler(std::move(indicationData));
            });
    }
};

} // namespace api
} // namespace hpm
} // namespace nx
