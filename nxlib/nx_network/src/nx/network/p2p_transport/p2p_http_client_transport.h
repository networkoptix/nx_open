#pragma once

#include <queue>
#include <nx/network/p2p_transport/i_p2p_transport.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/multipart_content_parser.h>

namespace nx::network {

class NX_NETWORK_API P2PHttpClientTransport: public IP2PTransport
{
private:
    using HttpClientPtr = std::unique_ptr<nx::network::http::AsyncClient>;

public:
    P2PHttpClientTransport(
        HttpClientPtr readHttpClient,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onPostConnectionEstablished,
        websocket::FrameType frameType = websocket::FrameType::binary);
    ~P2PHttpClientTransport();

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual SocketAddress getForeignAddress() const override;

private:
    enum class State
    {
        gotGetConnectionOnly,
        gotBothConnections,
        postConnectionFailed,
        failed
    };

    class PostBodySource: public http::AbstractMsgBodySource
    {
    public:
        PostBodySource(websocket::FrameType messageType, const Buffer& data);
        using CompletionHandler =
            utils::MoveOnlyFunc<void(SystemError::ErrorCode, http::BufferType)>;
        virtual http::StringType mimeType() const override;
        virtual boost::optional<uint64_t> contentLength() const override;
        virtual void readAsync(CompletionHandler completionHandler) override;

    private:
        websocket::FrameType m_messageType;
        nx::Buffer m_data;
    };

    HttpClientPtr m_writeHttpClient;
    HttpClientPtr m_readHttpClient;
    http::MultipartContentParser m_multipartContentParser;
    std::queue<nx::Buffer> m_incomingMessageQueue;
    std::queue<std::pair<nx::Buffer* const, IoCompletionHandler>> m_userReadQueue;
    std::queue<std::pair<nx::Buffer, IoCompletionHandler>> m_userWriteQueue;
    websocket::FrameType m_messageType;
    State m_state = State::gotGetConnectionOnly;

    void startEstablishingPostConnection(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onPostConnectionEstablished);
    void startReading();
    void handoutIncomingMessage(nx::Buffer* const buffer, IoCompletionHandler handler);
    void doPost(const nx::utils::Url& url, utils::MoveOnlyFunc<void(bool)> doneHandler,
        const nx::Buffer& data = nx::Buffer());
};

} // namespace nx::network
