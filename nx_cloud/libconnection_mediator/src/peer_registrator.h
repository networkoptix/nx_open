/**********************************************************
* 9 sep 2014
* a.kolesnikov
***********************************************************/

#pragma once

#include <functional>

#include <nx/fusion/serialization/lexical.h>
#include <nx/network/cloud/data/client_bind_data.h>
#include <nx/network/cloud/data/connection_requested_event_data.h>
#include <nx/network/cloud/data/listen_data.h>
#include <nx/network/cloud/data/resolve_domain_data.h>
#include <nx/network/cloud/data/resolve_peer_data.h>
#include <nx/network/cloud/data/result_code.h>

#include "request_processor.h"
#include "settings.h"

namespace nx {

namespace stun {
class MessageDispatcher;
}

namespace hpm {

class ListeningPeerPool;

/** Registers peers which desire to accept cloud connections, resolves such peers address.
 */
class PeerRegistrator
:
    protected RequestProcessor
{
public:
    PeerRegistrator(
        const conf::Settings& settings,
        AbstractCloudDataProvider* cloudData,
        nx::stun::MessageDispatcher* dispatcher,
        ListeningPeerPool* const listeningPeerPool);

private:
    void bind(
        const ConnectionStrongRef& connection,
        stun::Message requestMessage);

    void listen(
        const ConnectionStrongRef& connection,
        api::ListenRequest requestData,
        stun::Message requestMessage,
        std::function<void(api::ResultCode)> completionHandler);

    void resolveDomain(
        const ConnectionStrongRef& connection,
        api::ResolveDomainRequest requestData,
        stun::Message requestMessage,
        std::function<void(
            api::ResultCode, api::ResolveDomainResponse)> completionHandler);

    void resolvePeer(
        const ConnectionStrongRef& connection,
        api::ResolvePeerRequest requestData,
        stun::Message requestMessage,
        std::function<void(
            api::ResultCode, api::ResolvePeerResponse)> completionHandler);

    void clientBind(
        const ConnectionStrongRef& connection,
        api::ClientBindRequest requestData,
        stun::Message requestMessage,
        std::function<void(api::ResultCode)> completionHandler);

private:
    struct ClientBindInfo
    {
        ConnectionWeakRef connection;
        nx::String originatingPeerID;
        std::list<SocketAddress> tcpReverseEndpoints;

        nx::stun::Message makeIndication(const conf::Settings& settings) const;
    };

    const conf::Settings& m_settings;

    mutable QnMutex m_mutex;
    std::list<ClientBindInfo> m_clientBindInfos;
    ListeningPeerPool* const m_listeningPeerPool;


};

} // namespace hpm
} // namespace nx
