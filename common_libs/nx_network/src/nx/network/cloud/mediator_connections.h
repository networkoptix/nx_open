#ifndef NX_CC_MEDIATOR_CONNECTIONS_H
#define NX_CC_MEDIATOR_CONNECTIONS_H

#include <nx/network/cloud/data/connection_ack_data.h>
#include <nx/network/cloud/data/connection_requested_event_data.h>
#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/cc/custom_stun.h>

#include "data/bind_data.h"
#include "data/connect_data.h"
#include "data/connection_result_data.h"
#include "data/listen_data.h"
#include "data/ping_data.h"
#include "data/resolve_data.h"
#include "data/result_code.h"


namespace nx {
namespace network {
namespace cloud {

// Forward
class MediatorConnector;

/** Provides helper functions for easy adding requests to mediator */
class NX_NETWORK_API BaseMediatorClient
:
    public stun::AsyncClientUser
{
public:
    BaseMediatorClient(std::shared_ptr<stun::AsyncClient> client);

protected:
    template<typename RequestData, typename CompletionHandlerType>
    void doRequest(
        nx::stun::cc::methods::Value method,
        RequestData requestData,
        CompletionHandlerType completionHandler);

    template<typename ResponseData>
    void sendRequestAndReceiveResponse(
        stun::Message request,
        std::function<void(nx::hpm::api::ResultCode, ResponseData)> completionHandler);

    void sendRequestAndReceiveResponse(
        stun::Message request,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler);
};

/** Provides client related STUN functionality.
    \note These requests DO NOT require authentication
 */
class NX_NETWORK_API MediatorClientConnection
    : public BaseMediatorClient
{
    MediatorClientConnection(std::shared_ptr<stun::AsyncClient> client);
    friend class MediatorConnector;

public:
    void resolve(
        nx::hpm::api::ResolveRequest resolveData,
        std::function<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolveResponse)> completionHandler);

    void connect(
        nx::hpm::api::ConnectRequest connectData,
        std::function<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ConnectResponse)> completionHandler);
    void connectionResult(
        nx::hpm::api::ConnectionResultRequest resultData,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler);
};

/** Provides server-related STUN functionality.
    \note All server requests MUST be authorized by cloudId and cloudAuthenticationKey
*/
class NX_NETWORK_API MediatorServerConnection
    : public BaseMediatorClient
{
    MediatorServerConnection(std::shared_ptr<stun::AsyncClient> client,
                             MediatorConnector* connector);
    friend class MediatorConnector;

public:
    /** Ask mediator to test connection to addresses.
        \return list of endpoints available to the mediator
    */
    void ping(
        nx::hpm::api::PingRequest requestData,
        std::function<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::PingResponse)> completionHandler);

    /** reports to mediator that local server is available on \a addresses */
    void bind(
        nx::hpm::api::BindRequest requestData,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler);

    /** notifies mediator this server is willing to accept cloud connections */
    void listen(
        nx::hpm::api::ListenRequest listenParams,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler);
    /** server uses this request to confirm its willingness to proceed with cloud connection */
    void connectionAck(
        nx::hpm::api::ConnectionAckRequest request,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler);

    void setOnConnectionRequestedHandler(
        std::function<void(nx::hpm::api::ConnectionRequestedEvent)> handler);

protected:
    template<typename RequestData, typename CompletionHandlerType>
    void doAuthRequest(
        nx::stun::cc::methods::Value method,
        RequestData requestData,
        CompletionHandlerType completionHandler);

private:
    void sendAuthRequest(stun::Message request,
                         stun::AsyncClient::RequestHandler handler);

    MediatorConnector* m_connector;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_MEDIATOR_CONNECTIONS_H
