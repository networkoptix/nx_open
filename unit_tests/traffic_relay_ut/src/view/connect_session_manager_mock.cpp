#include "connect_session_manager_mock.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

void ConnectSessionManagerMock::beginListening(
    const api::BeginListeningRequest& /*request*/,
    BeginListeningHandler completionHandler)
{
    completionHandler(
        api::ResultCode::ok,
        api::BeginListeningResponse(),
        nx_http::ConnectionEvents());
}

void ConnectSessionManagerMock::createClientSession(
    const api::CreateClientSessionRequest& /*request*/,
    CreateClientSessionHandler completionHandler)
{
    completionHandler(
        api::ResultCode::ok,
        api::CreateClientSessionResponse());
}

void ConnectSessionManagerMock::connectToPeer(
    const api::ConnectToPeerRequest& /*request*/,
    ConnectToPeerHandler completionHandler)
{
    completionHandler(
        api::ResultCode::ok,
        nx_http::ConnectionEvents());

}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
