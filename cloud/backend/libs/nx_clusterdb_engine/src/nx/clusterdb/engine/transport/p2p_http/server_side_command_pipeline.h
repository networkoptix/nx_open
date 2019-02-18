#pragma once

#include <deque>
#include <optional>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/multipart_msg_body_source.h>

#include <nx/p2p/transport/i_p2p_transport.h>

namespace nx::clusterdb::engine::transport::p2p::http {

class ServerSideCommandPipeline:
    public nx::p2p::IP2PTransport
{
    using base_type = nx::p2p::IP2PTransport;

public:
    ServerSideCommandPipeline(
        nx::network::http::MultipartMessageBodySource* multipartMessageBody,
        const std::string& mimeType,
        const nx::network::SocketAddress& remotePeerEndpoint);

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        nx::network::IoCompletionHandler handler) override;

    virtual void sendAsync(
        const nx::Buffer& buffer,
        nx::network::IoCompletionHandler handler) override;

    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;

    virtual network::SocketAddress getForeignAddress() const override;

    virtual void start(
        utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart = nullptr) override;

    /**
     * @param message Will be reported as read by ServerSideCommandPipeline::readSomeAsync.
     */
    void saveReceivedMessage(nx::Buffer message);

protected:
    virtual void stopWhileInAioThread() override;

private:
    struct ReadContext
    {
        nx::Buffer* buffer = nullptr;
        nx::network::IoCompletionHandler handler;
    };

    nx::network::http::MultipartMessageBodySource* m_multipartMessageBody = nullptr;
    std::string m_mimeType;
    const nx::network::SocketAddress m_remotePeerEndpoint;
    nx::network::aio::BasicPollable m_sendExecutor;
    bool m_eof = false;
    std::deque<nx::Buffer> m_readQueue;
    std::optional<ReadContext> m_pendingRead;

    void markConnectonAsClosed();
    void reportNextCommand();
};

} // namespace nx::clusterdb::engine::transport::p2p::http
