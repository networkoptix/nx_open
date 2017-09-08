#include "connect_session_manager_mock.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

ConnectSessionManagerMock::ConnectSessionManagerMock(
    //utils::SyncQueue<api::BeginListeningRequest>* receivedBeginListeningRequests,
    utils::SyncQueue<api::CreateClientSessionRequest>* receivedCreateClientSessionRequests,
    utils::SyncQueue<api::ConnectToPeerRequest>* receivedConnectToPeerRequests)
    :
    //m_receivedBeginListeningRequests(receivedBeginListeningRequests),
    m_receivedCreateClientSessionRequests(receivedCreateClientSessionRequests),
    m_receivedConnectToPeerRequests(receivedConnectToPeerRequests)
{
}

//void ConnectSessionManagerMock::beginListening(
//    const api::BeginListeningRequest& request,
//    BeginListeningHandler completionHandler)
//{
//    m_receivedBeginListeningRequests->push(request);
//
//    completionHandler(
//        api::ResultCode::ok,
//        api::BeginListeningResponse(),
//        nx_http::ConnectionEvents());
//}

void ConnectSessionManagerMock::createClientSession(
    const api::CreateClientSessionRequest& request,
    CreateClientSessionHandler completionHandler)
{
    m_receivedCreateClientSessionRequests->push(std::move(request));

    completionHandler(
        api::ResultCode::ok,
        api::CreateClientSessionResponse());
}

void ConnectSessionManagerMock::connectToPeer(
    const api::ConnectToPeerRequest& request,
    ConnectToPeerHandler completionHandler)
{
    m_receivedConnectToPeerRequests->push(std::move(request));

    completionHandler(
        api::ResultCode::ok,
        nx_http::ConnectionEvents());
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
