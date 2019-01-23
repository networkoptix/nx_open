#pragma once

#include <nx/network/http/multipart_msg_body_source.h>

#include <nx/p2p/transport/i_p2p_transport.h>

namespace nx::clusterdb::engine::transport::p2p::http {

class ServerSideCommandPipeline:
    public nx::p2p::IP2PTransport
{
public:
    ServerSideCommandPipeline(
        nx::network::http::MultipartMessageBodySource* multipartMessageBody);

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

private:
    nx::network::http::MultipartMessageBodySource* m_multipartMessageBody = nullptr;
};

} // namespace nx::clusterdb::engine::transport::p2p::http
