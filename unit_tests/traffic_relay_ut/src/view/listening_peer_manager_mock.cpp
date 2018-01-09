#include "listening_peer_manager_mock.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

ListeningPeerManagerMock::ListeningPeerManagerMock(
    utils::SyncQueue<api::BeginListeningRequest>* receivedBeginListeningRequests)
    :
    m_receivedBeginListeningRequests(receivedBeginListeningRequests)
{
}

void ListeningPeerManagerMock::beginListening(
    const api::BeginListeningRequest& request,
    BeginListeningHandler completionHandler)
{
    m_receivedBeginListeningRequests->push(request);

    completionHandler(
        api::ResultCode::ok,
        api::BeginListeningResponse(),
        nx::network::http::ConnectionEvents());
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
