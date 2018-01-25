#pragma once

#include <functional>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/data/get_connection_state_data.h>
#include <nx/network/cloud/data/client_bind_data.h>
#include <nx/network/cloud/data/connection_requested_event_data.h>
#include <nx/network/cloud/data/listen_data.h>
#include <nx/network/cloud/data/resolve_domain_data.h>
#include <nx/network/cloud/data/resolve_peer_data.h>
#include <nx/network/cloud/data/result_code.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/counter.h>
#include <nx/network/cloud/mediator/api/listening_peer.h>

#include "request_processor.h"
#include "settings.h"

namespace nx { namespace hpm { namespace data { struct ListeningPeers; } } }

namespace nx {
namespace hpm {

class AbstractRelayClusterClient;
class ListeningPeerPool;

/**
 * Registers peers which desire to accept cloud connections, resolves such peers address.
 */
class PeerRegistrator:
    protected RequestProcessor
{
public:
    PeerRegistrator(
        const conf::Settings& settings,
        AbstractCloudDataProvider* cloudData,
        ListeningPeerPool* const listeningPeerPool,
        AbstractRelayClusterClient* const relayClusterClient);
    virtual ~PeerRegistrator() override;

    api::ListeningPeers getListeningPeers() const;

    void bind(
        const ConnectionStrongRef& connection,
        network::stun::Message requestMessage);

    void listen(
        const ConnectionStrongRef& connection,
        api::ListenRequest requestData,
        network::stun::Message requestMessage,
        std::function<void(api::ResultCode, api::ListenResponse)> completionHandler);

    void checkOwnState(
        const ConnectionStrongRef& connection,
        api::GetConnectionStateRequest requestData,
        network::stun::Message requestMessage,
        std::function<void(api::ResultCode, api::GetConnectionStateResponse)> completionHandler);

    void resolveDomain(
        const ConnectionStrongRef& connection,
        api::ResolveDomainRequest requestData,
        network::stun::Message requestMessage,
        std::function<void(
            api::ResultCode, api::ResolveDomainResponse)> completionHandler);

    void resolvePeer(
        const ConnectionStrongRef& connection,
        api::ResolvePeerRequest requestData,
        network::stun::Message requestMessage,
        std::function<void(
            api::ResultCode, api::ResolvePeerResponse)> completionHandler);

    void clientBind(
        const ConnectionStrongRef& connection,
        api::ClientBindRequest requestData,
        network::stun::Message requestMessage,
        std::function<void(api::ResultCode, api::ClientBindResponse)> completionHandler);

private:
    struct ClientBindInfo
    {
        ConnectionWeakRef connection;
        std::list<network::SocketAddress> tcpReverseEndpoints;
    };

    using BoundClients = std::map<nx::String, ClientBindInfo>;

    const conf::Settings& m_settings;
    mutable QnMutex m_mutex;
    BoundClients m_boundClients;
    ListeningPeerPool* const m_listeningPeerPool;
    AbstractRelayClusterClient* const m_relayClusterClient;
    nx::utils::Counter m_counter;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;

    void sendListenResponse(
        const ConnectionStrongRef& connection,
        boost::optional<QUrl> trafficRelayInstanceUrl,
        std::function<void(api::ResultCode, api::ListenResponse)> responseSender);
    void sendClientBindIndications(const ConnectionStrongRef& connection);
    nx::network::stun::Message makeIndication(const String& id, const ClientBindInfo& info) const;
};

} // namespace hpm
} // namespace nx
