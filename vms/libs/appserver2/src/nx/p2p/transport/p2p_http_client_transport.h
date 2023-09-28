// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <queue>

#include <nx/network/abstract_socket.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/websocket/websocket_common_types.h>
#include <nx/p2p/transport/i_p2p_transport.h>
#include <nx/string.h>
#include <nx/utils/interruption_flag.h>

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
        const nx::String& connectionGuid,
        network::websocket::FrameType frameType,
        const nx::utils::Url& url,
        std::optional<std::chrono::milliseconds> pingTimeout);

    virtual ~P2PHttpClientTransport() override;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        network::IoCompletionHandler handler) override;
    virtual void sendAsync(
        const nx::Buffer* buffer,
        network::IoCompletionHandler handler) override;
    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override;
    virtual network::aio::AbstractAioThread* getAioThread() const override;
    network:: SocketAddress getForeignAddress() const override;
    virtual void start(
        utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart = nullptr) override;

    // For tests.
    static void setPingTimeout(std::optional<std::chrono::milliseconds> pingTimeout);
    static void setPingPongDisabled(bool value);

private:
    class PostBodySource: public network::http::AbstractMsgBodySource
    {
    public:
        PostBodySource(network::websocket::FrameType messageType, const nx::Buffer& data);

        using CompletionHandler =
            utils::MoveOnlyFunc<void(SystemError::ErrorCode, nx::Buffer)>;
        virtual std::string mimeType() const override;
        virtual std::optional<uint64_t> contentLength() const override;
        virtual void readAsync(CompletionHandler completionHandler) override;

    private:
        network::websocket::FrameType m_messageType;
        const nx::Buffer m_data;
    };

    struct OutgoingData
    {
        std::optional<nx::Buffer> buffer;
        network::IoCompletionHandler handler;
        network::http::HttpHeaders headers;
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
    utils::Url m_url;
    utils::InterruptionFlag m_destructionFlag;
    nx::String m_connectionGuid;
    utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onStartHandler;
    std::optional<std::chrono::milliseconds> m_pingTimeout;
    network::aio::Timer m_timer;
    std::queue<OutgoingData> m_outgoingMessageQueue;
    bool m_sendInProgress = false;

    // For tests.
    static std::optional<std::chrono::milliseconds> s_pingTimeout;
    static bool s_pingPongDisabled;

    void startReading();
    void stopOrResumeReaderWhileInAioThread();
    virtual void stopWhileInAioThread() override;
    std::optional<std::chrono::milliseconds> pingTimeout() const;
    void sendPingOrPong(const std::string& name);
    void initiatePingPong();
    void setFailedState();
    void sendNextMessage();
};

} // namespace nx::network
