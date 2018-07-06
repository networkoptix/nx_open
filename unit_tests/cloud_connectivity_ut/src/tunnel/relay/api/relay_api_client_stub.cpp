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

ClientImpl::ClientImpl():
    m_scheduledRequestCount(0)
{
}

ClientImpl::~ClientImpl()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();

    // Notifying test fixture
    auto handler = std::move(m_onBeforeDestruction);
    if (handler)
        handler();
}

void ClientImpl::beginListening(
    const nx::String& /*peerName*/,
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

void ClientImpl::startSession(
    const nx::String& desiredSessionId,
    const nx::String& /*targetPeerName*/,
    nx::cloud::relay::api::StartClientConnectSessionHandler handler)
{
    ++m_scheduledRequestCount;
    if (m_behavior == RequestProcessingBehavior::ignore)
        return;

    post(
        [this, handler = std::move(handler), desiredSessionId]()
        {
            nx::cloud::relay::api::CreateClientSessionResponse response;
            response.sessionId = desiredSessionId.toStdString();
            if (m_behavior == RequestProcessingBehavior::succeed)
                handler(nx::cloud::relay::api::ResultCode::ok, std::move(response));
            else
                handler(nx::cloud::relay::api::ResultCode::networkError, std::move(response));
        });
}

void ClientImpl::openConnectionToTheTargetHost(
    const nx::String& /*sessionId*/,
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

utils::Url ClientImpl::url() const
{
    return nx::utils::Url();
}

SystemError::ErrorCode ClientImpl::prevRequestSysErrorCode() const
{
    return SystemError::noError;
}

int ClientImpl::scheduledRequestCount() const
{
    return m_scheduledRequestCount;
}

void ClientImpl::setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onBeforeDestruction = std::move(handler);
}

void ClientImpl::setBehavior(RequestProcessingBehavior behavior)
{
    m_behavior = behavior;
}

void ClientImpl::setIgnoreRequests()
{
    m_behavior = RequestProcessingBehavior::ignore;
}

void ClientImpl::setFailRequests()
{
    m_behavior = RequestProcessingBehavior::fail;
}

void ClientImpl::stopWhileInAioThread()
{
    // TODO
}

} // namespace test
} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
