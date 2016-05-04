/**********************************************************
* May 4, 2016
* a.kolesnikov
***********************************************************/

#include "get_listening_peer_list_handler.h"

#include "listening_peer_pool.h"


namespace nx {
namespace hpm {
namespace http {

GetListeningPeerListHandler::GetListeningPeerListHandler(
    const ListeningPeerPool& listeningPeerPool)
:
    m_listeningPeerPool(listeningPeerPool)
{
}

void GetListeningPeerListHandler::processRequest(
    const nx_http::HttpServerConnection& /*connection*/,
    const nx_http::Request& /*request*/,
    stree::ResourceContainer /*authInfo*/)
{
    requestCompleted(
        nx_http::FusionRequestResult(),
        m_listeningPeerPool.getListeningPeers());
}

} // namespace http
} // namespace hpm
} // namespace nx
