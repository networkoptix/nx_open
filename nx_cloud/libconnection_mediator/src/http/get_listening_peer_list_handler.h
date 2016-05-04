/**********************************************************
* May 4, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <nx/network/http/server/abstract_fusion_request_handler.h>

#include "data/listening_peer.h"


namespace nx {
namespace hpm {

class ListeningPeerPool;

namespace http {

class GetListeningPeerListHandler
:
    public nx_http::AbstractFusionRequestHandler<void, data::ListeningPeersBySystem>
{
public:
    static const char* kHandlerPath;

    GetListeningPeerListHandler(const ListeningPeerPool& listeningPeerPool);

    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        stree::ResourceContainer authInfo) override;

private:
    const ListeningPeerPool& m_listeningPeerPool;
};

} // namespace http
} // namespace hpm
} // namespace nx
