#include "server_side_command_pipeline.h"

namespace nx::clusterdb::engine::transport::p2p::http {

ServerSideCommandPipeline::ServerSideCommandPipeline(
    nx::network::http::MultipartMessageBodySource* multipartMessageBody,
    const std::string& mimeType,
    const nx::network::SocketAddress& remotePeerEndpoint)
    :
    m_multipartMessageBody(multipartMessageBody),
    m_mimeType(mimeType),
    m_remotePeerEndpoint(remotePeerEndpoint)
{
    bindToAioThread(m_multipartMessageBody->getAioThread());

    m_multipartMessageBody->setOnBeforeDestructionHandler(
        [this]() { markConnectonAsClosed(); });
}

void ServerSideCommandPipeline::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_sendExecutor.bindToAioThread(aioThread);
}

void ServerSideCommandPipeline::readSomeAsync(
    nx::Buffer* const buffer,
    nx::network::IoCompletionHandler handler)
{
    post(
        [this, buffer, handler = std::move(handler)]() mutable
        {
            if (m_eof)
                return handler(SystemError::connectionReset, -1);

            NX_CRITICAL(!m_pendingRead);
            m_pendingRead = {buffer, std::move(handler)};

            if (!m_readQueue.empty())
                reportNextCommand();
        });
}

void ServerSideCommandPipeline::sendAsync(
    const nx::Buffer& buffer,
    nx::network::IoCompletionHandler handler)
{
    m_sendExecutor.post(
        [this, buffer = buffer.toBase64(), handler = std::move(handler)]()
        {
            if (m_eof)
                return handler(SystemError::connectionReset, -1);

            m_multipartMessageBody->serializer()->writeBodyPart(
                m_mimeType.c_str(),
                {},
                buffer);
            handler(SystemError::noError, buffer.size());
        });
}

void ServerSideCommandPipeline::cancelIoInAioThread(
    nx::network::aio::EventType eventType)
{
    using namespace nx::network::aio;

    if (eventType == EventType::etRead || eventType == EventType::etNone)
        m_pendingRead = std::nullopt;
    if (eventType == EventType::etWrite || eventType == EventType::etNone)
        m_sendExecutor.cancelPostedCallsSync();
}

network::SocketAddress ServerSideCommandPipeline::getForeignAddress() const
{
    return m_remotePeerEndpoint;
}

void ServerSideCommandPipeline::start(
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    if (!handler)
        return;

    post([handler = std::move(handler)]() { handler(SystemError::noError); });
}

void ServerSideCommandPipeline::saveReceivedCommandBuffer(nx::Buffer command)
{
    post(
        [this, command = std::move(command)]()
        {
            m_readQueue.push_back(std::move(command));

            if (m_pendingRead)
                reportNextCommand();
        });
}

void ServerSideCommandPipeline::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_sendExecutor.pleaseStopSync();
}

void ServerSideCommandPipeline::markConnectonAsClosed()
{
    NX_ASSERT(isInSelfAioThread());

    m_eof = true;
    
    if (m_pendingRead)
    {
        auto pendingRead = std::exchange(m_pendingRead, std::nullopt);
        nx::utils::swapAndCall(pendingRead->handler, SystemError::connectionReset, -1);
    }
}

void ServerSideCommandPipeline::reportNextCommand()
{
    NX_CRITICAL(!m_readQueue.empty());
    NX_CRITICAL(m_pendingRead);

    auto pendingRead = std::exchange(m_pendingRead, std::nullopt);

    *pendingRead->buffer = m_readQueue.front();
    m_readQueue.pop_front();
    nx::utils::swapAndCall(
        pendingRead->handler,
        SystemError::noError,
        pendingRead->buffer->size());
}

} // namespace nx::clusterdb::engine::transport::p2p::http
