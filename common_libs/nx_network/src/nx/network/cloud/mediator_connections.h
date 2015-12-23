#ifndef NX_CC_MEDIATOR_CONNECTIONS_H
#define NX_CC_MEDIATOR_CONNECTIONS_H

#include <nx/network/stun/async_client_user.h>
#include <nx/network/stun/cc/custom_stun.h>

#include "data/connect_data.h"
#include "data/resolve_data.h"
#include "data/result_code.h"

namespace nx {
namespace network {
namespace cloud {

// Forward
class MediatorConnector;

/** Provides client related STUN functionality */
class NX_NETWORK_API MediatorClientConnection
    : public stun::AsyncClientUser
{
    MediatorClientConnection(std::shared_ptr<stun::AsyncClient> client);
    friend class MediatorConnector;

public:
    void connect(
        api::ConnectRequest connectData,
        std::function<void(api::ResultCode, api::ConnectResponse)> completionHandler);
    void resolve(
        api::ResolveRequest resolveData,
        std::function<void(api::ResultCode, api::ResolveResponse)> completionHandler);

private:
    template<typename RequestData, typename ResponseData>
    void doRequest(
        nx::stun::cc::methods::Value method,
        RequestData requestData,
        std::function<void(api::ResultCode, ResponseData)> completionHandler);
};

/** Provides system related STUN functionality */
class NX_NETWORK_API MediatorSystemConnection
    : public stun::AsyncClientUser
{
    MediatorSystemConnection(std::shared_ptr<stun::AsyncClient> client,
                             MediatorConnector* connector);
    friend class MediatorConnector;

public:
    void ping(std::list<SocketAddress> addresses,
              std::function<void(bool, std::list<SocketAddress>)> handler);

    void bind(std::list<SocketAddress> addresses,
              std::function<void(api::ResultCode, bool)> handler);

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
