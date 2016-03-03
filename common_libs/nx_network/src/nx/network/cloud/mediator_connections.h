#ifndef NX_CC_MEDIATOR_CONNECTIONS_H
#define NX_CC_MEDIATOR_CONNECTIONS_H

#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <nx/network/stun/udp_client.h>

#include "abstract_cloud_system_credentials_provider.h"
#include "base_mediator_client.h"
#include "data/bind_data.h"
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

/** Provides access to mediator functions to be used by clients.
    \note These requests DO NOT require authentication
 */
template<class NetworkClientType>
class MediatorClientConnection
:
    public BaseMediatorClient<NetworkClientType>
{
public:
    //TODO #ak #msvc2015 variadic template
    template<typename Arg1Type>
        MediatorClientConnection(Arg1Type arg1)
    :
        BaseMediatorClient<NetworkClientType>(std::move(arg1))
    {
    }

    void resolveDomain(
        nx::hpm::api::ResolveDomainRequest resolveData,
        std::function<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolveDomainResponse)> completionHandler)
    {
        this->doRequest(
            stun::cc::methods::resolveDomain,
            std::move(resolveData),
            std::move(completionHandler));
    }

    void resolvePeer(
        nx::hpm::api::ResolvePeerRequest resolveData,
        std::function<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolvePeerResponse)> completionHandler)
    {
        this->doRequest(
            stun::cc::methods::resolvePeer,
            std::move(resolveData),
            std::move(completionHandler));
    }

    void connect(
        nx::hpm::api::ConnectRequest connectData,
        std::function<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ConnectResponse)> completionHandler)
    {
        this->doRequest(
            stun::cc::methods::connect,
            std::move(connectData),
            std::move(completionHandler));
    }

    void connectionResult(
        nx::hpm::api::ConnectionResultRequest resultData,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        this->doRequest(
            stun::cc::methods::connectionResult,
            std::move(resultData),
            std::move(completionHandler));
    }
};

typedef MediatorClientConnection<stun::AsyncClientUser> MediatorClientTcpConnection;
typedef MediatorClientConnection<stun::UDPClient> MediatorClientUdpConnection;

/** Provides access to mediator functions to be used by servers.
    \note All server requests MUST be authorized by cloudId and cloudAuthenticationKey
*/
template<class NetworkClientType>
class MediatorServerConnection
:
    public BaseMediatorClient<NetworkClientType>
{
public:
    //TODO #ak #msvc2015 variadic template
    template<typename Arg1Type>
    MediatorServerConnection(
        Arg1Type arg1,
        AbstractCloudSystemCredentialsProvider* connector)
    :
        BaseMediatorClient<NetworkClientType>(std::move(arg1)),
        m_connector(connector)
    {
    }

    /** Ask mediator to test connection to addresses.
        \return list of endpoints available to the mediator
    */
    void ping(
        nx::hpm::api::PingRequest requestData,
        std::function<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::PingResponse)> completionHandler)
    {
        this->doAuthRequest(
            stun::cc::methods::ping,
            std::move(requestData),
            std::move(completionHandler));
    }

    /** reports to mediator that local server is available on \a addresses */
    void bind(
        nx::hpm::api::BindRequest requestData,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        this->doAuthRequest(
            stun::cc::methods::bind,
            std::move(requestData),
            std::move(completionHandler));
    }

    /** notifies mediator this server is willing to accept cloud connections */
    void listen(
        nx::hpm::api::ListenRequest listenParams,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        this->doAuthRequest(
            stun::cc::methods::listen,
            std::move(listenParams),
            std::move(completionHandler));
    }

    /** server uses this request to confirm its willingness to proceed with cloud connection */
    void connectionAck(
        nx::hpm::api::ConnectionAckRequest request,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler)
    {
        this->doAuthRequest(
            stun::cc::methods::connectionAck,
            std::move(request),
            std::move(completionHandler));
    }

    String selfPeerId() const
    {
        if (m_connector)
            if (auto credentials = m_connector->getSystemCredentials())
                return credentials->systemId + String(".") + credentials->serverId;

        return String();
    }

    AbstractCloudSystemCredentialsProvider* credentialsProvider() const
    {
        return m_connector;
    }

protected:
    template<typename RequestData, typename CompletionHandlerType>
    void doAuthRequest(
        nx::stun::cc::methods::Value method,
        RequestData requestData,
        CompletionHandlerType completionHandler)
    {
        stun::Message request(
            stun::Header(
                stun::MessageClass::request,
                method));
        requestData.serialize(&request);

        if (auto credentials = m_connector->getSystemCredentials())
        {
            request.newAttribute<stun::cc::attrs::SystemId>(credentials->systemId);
            request.newAttribute<stun::cc::attrs::ServerId>(credentials->serverId);
            request.insertIntegrity(credentials->systemId, credentials->key);
        }

        this->sendRequestAndReceiveResponse(
            std::move(request),
            std::move(completionHandler));
    }

private:
    AbstractCloudSystemCredentialsProvider* m_connector;
};

typedef MediatorServerConnection<stun::UDPClient> MediatorServerUdpConnection;


class MediatorServerTcpConnection
:
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
            nx::stun::cc::indications::connectionRequested,
            [handler](nx::stun::Message msg)    //TODO #ak #msvc2015 move to lambda
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

#endif // NX_CC_MEDIATOR_CONNECTIONS_H
