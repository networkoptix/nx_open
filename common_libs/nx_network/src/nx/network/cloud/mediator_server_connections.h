#pragma once

#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/extension/stun_extension_types.h>
#include <nx/network/stun/udp_client.h>

#include "abstract_cloud_system_credentials_provider.h"
#include "base_mediator_client.h"
#include "data/bind_data.h"
#include "data/get_connection_state_data.h"
#include "data/connection_ack_data.h"
#include "data/connection_requested_event_data.h"
#include "data/listen_data.h"
#include "data/ping_data.h"

namespace nx {
namespace hpm {
namespace api {

/**
 * Provides access to mediator functions to be used by servers.
 * NOTE: All server requests MUST be authorized by cloudSystemId and cloudAuthenticationKey.
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
            std::move(requestData),
            std::move(completionHandler));
    }

    /**
     * Reports to mediator that local server is available on addresses.
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

    /**
     * Reads own state from mediator perspective.
     */
    void getConnectionState(
        utils::MoveOnlyFunc<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::GetConnectionStateResponse)> completionHandler)
    {
        this->doAuthRequest(
            GetConnectionStateRequest(),
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
        using namespace nx::network;

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

typedef MediatorServerConnection<network::stun::UdpClient> MediatorServerUdpConnection;

class NX_NETWORK_API MediatorServerTcpConnection:
    public MediatorServerConnection<network::stun::AsyncClientUser>
{
public:
    MediatorServerTcpConnection(
        std::shared_ptr<nx::network::stun::AbstractAsyncClient> stunClient,
        AbstractCloudSystemCredentialsProvider* connector);

    virtual ~MediatorServerTcpConnection() override;

    /**
     * @param handler will be called each time connect request is received.
     */
    void setOnConnectionRequestedHandler(
        std::function<void(nx::hpm::api::ConnectionRequestedEvent)> handler);

    /**
     * Verifies if current peer is in listening state. The connection will be closed in case
     *     if verification fails.
     * @param repeatPeriod timeout to check again.
     */
    void monitorListeningState(std::chrono::milliseconds repeatPeriod);
};

} // namespace api
} // namespace hpm
} // namespace nx
