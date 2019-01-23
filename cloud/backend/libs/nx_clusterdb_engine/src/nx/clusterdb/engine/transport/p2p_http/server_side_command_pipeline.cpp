#include "server_side_command_pipeline.h"

namespace nx::clusterdb::engine::transport::p2p::http {

ServerSideCommandPipeline::ServerSideCommandPipeline(
    nx::network::http::MultipartMessageBodySource* multipartMessageBody)
    :
    m_multipartMessageBody(multipartMessageBody)
{
}

void ServerSideCommandPipeline::readSomeAsync(
    nx::Buffer* const /*buffer*/,
    nx::network::IoCompletionHandler handler)
{
    handler(SystemError::notImplemented, -1);
    // TODO
}

void ServerSideCommandPipeline::sendAsync(
    const nx::Buffer& /*buffer*/,
    nx::network::IoCompletionHandler handler)
{
    handler(SystemError::notImplemented, -1);
    // TODO
}

void ServerSideCommandPipeline::cancelIoInAioThread(nx::network::aio::EventType /*eventType*/)
{
    // TODO
}

network::SocketAddress ServerSideCommandPipeline::getForeignAddress() const
{
    // TODO
    return network::SocketAddress();
}

void ServerSideCommandPipeline::start(
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart)
{
    onStart(SystemError::notImplemented);
    // TODO
}

} // namespace nx::clusterdb::engine::transport::p2p::http
