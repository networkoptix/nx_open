#pragma once

#include <queue>
#include <nx/p2p/transport/i_p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/utils/object_destruction_flag.h>

namespace nx::p2p {

class P2PHttpClientTransport: public IP2PTransport
{
private:
    using HttpClientPtr = std::unique_ptr<nx::network::http::AsyncClient>;

public:
    /**
     * @param url If set used instead of readHttpClient->url().
     */
    P2PHttpClientTransport(
        HttpClientPtr readHttpClient,
        const nx::Buffer& connectionGuid,
        network::websocket::FrameType frameType = network::websocket::FrameType::binary,
        const boost::optional<utils::Url>& url = boost::none);

    virtual ~P2PHttpClientTransport() override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        network::IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer& buffer,
        network::IoCompletionHandler handler) override;
    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual network::aio::AbstractAioThread* getAioThread() const override;
    network:: SocketAddress getForeignAddress() const override;
    virtual void start(
        utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart = nullptr) override;

private:
    class PostBodySource: public network::http::AbstractMsgBodySource
    {
    public:
        PostBodySource(network::websocket::FrameType messageType, const nx::Buffer& data);
        using CompletionHandler =
            utils::MoveOnlyFunc<void(SystemError::ErrorCode, network::http::BufferType)>;
        virtual network::http::StringType mimeType() const override;
        virtual boost::optional<uint64_t> contentLength() const override;
        virtual void readAsync(CompletionHandler completionHandler) override;

    private:
        network::websocket::FrameType m_messageType;
        const nx::Buffer m_data;
    };

    using UserReadHandlerPair =
        std::unique_ptr<std::pair<nx::Buffer* const, network::IoCompletionHandler>>;

    HttpClientPtr m_writeHttpClient;
    HttpClientPtr m_readHttpClient;
    network::http::MultipartContentParser m_multipartContentParser;
    std::queue<nx::Buffer> m_incomingMessageQueue;
    UserReadHandlerPair m_userReadHandlerPair;
    network::websocket::FrameType m_messageType;
    bool m_failed = false;
    boost::optional<utils::Url> m_url;
    utils::ObjectDestructionFlag m_destructionFlag;
    nx::Buffer m_connectionGuid;

    void startReading();
	void stopOrResumeReaderWhileInAioThread();
    virtual void stopWhileInAioThread() override;
};

} // namespace nx::network
