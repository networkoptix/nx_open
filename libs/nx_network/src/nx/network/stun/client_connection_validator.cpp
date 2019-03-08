#include "client_connection_validator.h"

#include <nx/utils/log/log.h>

namespace nx::network::stun {

ClientConnectionValidator::ClientConnectionValidator(
    std::unique_ptr<AbstractStreamSocket> connection)
    :
    m_messagePipeline(std::move(connection))
{
    bindToAioThread(m_messagePipeline.getAioThread());
}

void ClientConnectionValidator::bindToAioThread(
    aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_messagePipeline.bindToAioThread(aioThread);
}

void ClientConnectionValidator::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
}

void ClientConnectionValidator::validate(
    http::tunneling::ValidateTunnelCompletionHandler handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            NX_VERBOSE(this, "Validating STUN connection to %1",
                m_messagePipeline.socket()->getForeignAddress());

            m_completionHandler = std::move(handler);

            sendBindingRequest();

            m_messagePipeline.setMessageHandler(
                [this](auto message) { processMessage(std::move(message)); });
            m_messagePipeline.registerCloseHandler(
                [this](auto resultCode) { processConnectionClosure(resultCode); });
            m_messagePipeline.startReadingConnection(m_timeout);
        });
}

std::unique_ptr<AbstractStreamSocket> ClientConnectionValidator::takeConnection()
{
    if (m_connection)
        return std::exchange(m_connection, nullptr);

    return m_messagePipeline.takeSocket();
}

void ClientConnectionValidator::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_messagePipeline.pleaseStopSync();
}

void ClientConnectionValidator::processMessage(Message /*message*/)
{
    m_connection = m_messagePipeline.takeSocket();
    m_messagePipeline.pleaseStopSync();

    NX_VERBOSE(this, "STUN connection to %1 has been validated",
        m_connection->getForeignAddress());

    nx::utils::swapAndCall(m_completionHandler, http::tunneling::ResultCode::ok);
}

void ClientConnectionValidator::sendBindingRequest()
{
    m_messagePipeline.sendMessage(Message(
        Header(MessageClass::request, MethodType::bindingMethod)));
}

void ClientConnectionValidator::processConnectionClosure(
    SystemError::ErrorCode reasonCode)
{
    NX_DEBUG(this, "Failed to validate STUN connection to %1. %2",
        m_messagePipeline.socket()->getForeignAddress(), SystemError::toString(reasonCode));

    m_messagePipeline.pleaseStopSync();

    if (m_completionHandler)
        nx::utils::swapAndCall(m_completionHandler, http::tunneling::ResultCode::ioError);
}

} // namespace nx::network::stun
