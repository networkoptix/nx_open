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
    /**
     * @param url If set used instead of readHttpClient->url().
     */
    P2PHttpClientTransport(
        HttpClientPtr readHttpClient,
        websocket::FrameType frameType = websocket::FrameType::binary,
        const boost::optional<utils::Url>& url = boost::none);

    virtual ~P2PHttpClientTransport() override;

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual aio::AbstractAioThread* getAioThread() const override;
    virtual void pleaseStopSync() override;
    virtual SocketAddress getForeignAddress() const override;

private:
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
        const nx::Buffer& m_data;
    };

    enum class FrameContentType
    {
        payload,
        dummy
    };

    using UserReadHandlePair = std::unique_ptr<std::pair<nx::Buffer* const, IoCompletionHandler>>;

    HttpClientPtr m_writeHttpClient;
    HttpClientPtr m_readHttpClient;
    http::MultipartContentParser m_multipartContentParser;
    std::queue<nx::Buffer> m_incomingMessageQueue;
    UserReadHandlePair m_userReadHandlerPair;
    bool m_postInProgress = false;
    websocket::FrameType m_messageType;
    bool m_failed = false;
    boost::optional<utils::Url> m_url;

    void startReading();
};

} // namespace nx::network
