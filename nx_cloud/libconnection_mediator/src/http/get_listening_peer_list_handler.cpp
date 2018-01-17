#include "get_listening_peer_list_handler.h"

#include "peer_registrator.h"

namespace nx {
namespace hpm {
namespace http {

const char* GetListeningPeerListHandler::kHandlerPath = "/mediator/statistics/listening_peers";

GetListeningPeerListHandler::GetListeningPeerListHandler(const PeerRegistrator& registrator):
    m_registrator(registrator)
{
}

void GetListeningPeerListHandler::processRequest(
    nx::network::http::HttpServerConnection* const /*connection*/,
    const nx::network::http::Request& /*request*/,
    nx::utils::stree::ResourceContainer /*authInfo*/)
{
    requestCompleted(nx::network::http::FusionRequestResult(), m_registrator.getListeningPeers());
}

} // namespace http
} // namespace hpm
} // namespace nx
