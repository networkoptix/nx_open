#ifndef NX_CC_MEDIATOR_CONNECTIONS_H
#define NX_CC_MEDIATOR_CONNECTIONS_H

#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/cc/custom_stun.h>

#include "data/connect_data.h"
#include "data/listen_data.h"
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
    template<typename RequestData, typename ResponseData>
    void doRequest(
        nx::stun::cc::methods::Value method,
        RequestData requestData,
        std::function<void(nx::hpm::api::ResultCode, ResponseData)> completionHandler);

    template<typename RequestData>
    void doRequest(
        nx::stun::cc::methods::Value method,
        RequestData requestData,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler);
};

/** Provides client related STUN functionality */
class NX_NETWORK_API MediatorClientConnection
    : public BaseMediatorClient
{
    MediatorClientConnection(std::shared_ptr<stun::AsyncClient> client);
    friend class MediatorConnector;

public:
    //void connect(
    //    nx::hpm::api::ConnectRequest connectData,
    //    std::function<void(
    //        nx::hpm::api::ResultCode,
    //        nx::hpm::api::ConnectResponse)> completionHandler);
    void resolve(
        nx::hpm::api::ResolveRequest resolveData,
        std::function<void(
            nx::hpm::api::ResultCode,
            nx::hpm::api::ResolveResponse)> completionHandler);
};

/** Provides system related STUN functionality */
class NX_NETWORK_API MediatorSystemConnection
    : public BaseMediatorClient
{
    MediatorSystemConnection(std::shared_ptr<stun::AsyncClient> client,
                             MediatorConnector* connector);
    friend class MediatorConnector;

public:
    void ping(std::list<SocketAddress> addresses,
              std::function<void(bool, std::list<SocketAddress>)> handler);

    /** reports to mediator that local server is available on \a addresses */
    void bind(std::list<SocketAddress> addresses,
              std::function<void(nx::hpm::api::ResultCode)> handler);
    /** notifies mediator this server is willing to accept cloud connections */
    void listen(
        nx::hpm::api::ListenRequest listenParams,
        std::function<void(nx::hpm::api::ResultCode)> completionHandler);

    // TODO: propper implementation
    typedef int ConnectionRequest;
    void monitorConnectionRequest(std::function<void(ConnectionRequest)> handler);

    // TODO: propper implementation
    typedef int ConnectionUpdate;
    void monitorConnectionUpdate(std::function<void(ConnectionUpdate)> handler);

private:
    void sendAuthRequest(stun::Message request,
                         stun::AsyncClient::RequestHandler handler);

    MediatorConnector* m_connector;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_MEDIATOR_CONNECTIONS_H
