#pragma once

#include <nx/network/http/server/abstract_fusion_request_handler.h>

#include "data/listening_peer.h"

namespace nx {
namespace hpm {

class PeerRegistrator;

namespace http {

class GetListeningPeerListHandler
:
    public nx::network::http::AbstractFusionRequestHandler<void, data::ListeningPeers>
{
public:
    static const char* kHandlerPath;

    GetListeningPeerListHandler(const PeerRegistrator& registrator);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        const nx::network::http::Request& request,
        nx::utils::stree::ResourceContainer authInfo) override;

private:
    const PeerRegistrator& m_registrator;
};

} // namespace http
} // namespace hpm
} // namespace nx
