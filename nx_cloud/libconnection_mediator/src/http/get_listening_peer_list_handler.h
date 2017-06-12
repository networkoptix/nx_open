#pragma once

#include <nx/network/http/server/abstract_fusion_request_handler.h>

#include "data/listening_peer.h"

namespace nx {
namespace hpm {

class PeerRegistrator;

namespace http {

class GetListeningPeerListHandler
:
    public nx_http::AbstractFusionRequestHandler<void, data::ListeningPeers>
{
public:
    static const char* kHandlerPath;

    GetListeningPeerListHandler(const PeerRegistrator& registrator);

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo) override;

private:
    const PeerRegistrator& m_registrator;
};

} // namespace http
} // namespace hpm
} // namespace nx
