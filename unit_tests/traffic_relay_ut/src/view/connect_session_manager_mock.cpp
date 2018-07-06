#include "connect_session_manager_mock.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

ConnectSessionManagerMock::ConnectSessionManagerMock(
    utils::SyncQueue<api::CreateClientSessionRequest>* receivedCreateClientSessionRequests,
    utils::SyncQueue<api::ConnectToPeerRequest>* receivedConnectToPeerRequests)
    :
    m_receivedCreateClientSessionRequests(receivedCreateClientSessionRequests),
    m_receivedConnectToPeerRequests(receivedConnectToPeerRequests)
{
}

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
    const controller::ConnectToPeerRequestEx& request,
    ConnectToPeerHandler completionHandler)
{
    m_receivedConnectToPeerRequests->push(std::move(request));

    completionHandler(
        api::ResultCode::ok,
        nx::network::http::ConnectionEvents());
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
