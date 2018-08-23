#include "relay_api_client_stub.h"

#include <nx/network/aio/aio_service.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {
namespace test {

ClientStub::ClientStub(const nx::utils::Url& /*relayUrl*/):
    m_scheduledRequestCount(0)
{
}

ClientStub::~ClientStub()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();

    // Notifying test fixture
    auto handler = std::move(m_onBeforeDestruction);
    if (handler)
        handler();
}

void ClientStub::beginListening(
    const std::string& /*peerName*/,
    nx::cloud::relay::api::BeginListeningHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_behavior == RequestProcessingBehavior::ignore)
        return;

    post(
        [this, handler = std::move(handler)]()
        {
            nx::cloud::relay::api::BeginListeningResponse response;
            response.preemptiveConnectionCount = 7;
            auto resultCode = m_behavior == RequestProcessingBehavior::succeed
                ? nx::cloud::relay::api::ResultCode::ok
                : nx::cloud::relay::api::ResultCode::networkError;
            handler(resultCode, std::move(response), nullptr);
        });
}

void ClientStub::startSession(
    const std::string& desiredSessionId,
    const std::string& /*targetPeerName*/,
    nx::cloud::relay::api::StartClientConnectSessionHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_behavior == RequestProcessingBehavior::ignore)
        return;

    post(
        [this, handler = std::move(handler), desiredSessionId]()
        {
            nx::cloud::relay::api::CreateClientSessionResponse response;
            response.sessionId = desiredSessionId;
            if (m_behavior == RequestProcessingBehavior::succeed)
                handler(nx::cloud::relay::api::ResultCode::ok, std::move(response));
            else
                handler(nx::cloud::relay::api::ResultCode::networkError, std::move(response));
        });
}

void ClientStub::openConnectionToTheTargetHost(
    const std::string& /*sessionId*/,
    nx::cloud::relay::api::OpenRelayConnectionHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_behavior == RequestProcessingBehavior::ignore)
        return;

    post(
        [this, handler = std::move(handler)]()
        {
            if (m_behavior == RequestProcessingBehavior::succeed)
            {
                auto connection = std::make_unique<network::cloud::CloudStreamSocket>();
                connection->bindToAioThread(
                    network::SocketGlobals::aioService().getCurrentAioThread());
                handler(
                    nx::cloud::relay::api::ResultCode::ok,
                    std::move(connection));
            }
            else if (m_behavior == RequestProcessingBehavior::produceLogicError)
            {
                auto connection = std::make_unique<network::cloud::CloudStreamSocket>();
                connection->bindToAioThread(
                    network::SocketGlobals::aioService().getCurrentAioThread());
                handler(
                    nx::cloud::relay::api::ResultCode::notFound,
                    std::move(connection));
            }
            else
            {
                handler(
                    nx::cloud::relay::api::ResultCode::networkError,
                    nullptr);
            }
        });
}

utils::Url ClientStub::url() const
{
    return nx::utils::Url();
}

SystemError::ErrorCode ClientStub::prevRequestSysErrorCode() const
{
    return SystemError::noError;
}

int ClientStub::scheduledRequestCount() const
{
    return m_scheduledRequestCount;
}

void ClientStub::setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onBeforeDestruction = std::move(handler);
}

void ClientStub::setBehavior(RequestProcessingBehavior behavior)
{
    m_behavior = behavior;
}

void ClientStub::setIgnoreRequests()
{
    m_behavior = RequestProcessingBehavior::ignore;
}

void ClientStub::setFailRequests()
{
    m_behavior = RequestProcessingBehavior::fail;
}

void ClientStub::stopWhileInAioThread()
{
    // TODO
}

} // namespace test
} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
